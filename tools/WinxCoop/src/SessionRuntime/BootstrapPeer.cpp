#include "BootstrapPeer.h"
#include "Progression/ProgressionProtocol.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace winxcoop::bootstrap
{
    namespace n = networking;
    namespace p = progression;
    namespace
    {
        constexpr std::uint16_t CarrierStartToken = 0xE701;
        n::TcpTransportConfig Transport(const Config& config)
        {
            auto result = config.transport;
            result.role = config.role == Role::Host ? n::SessionRole::Host : n::SessionRole::Guest;
            return result;
        }
        bool ValidRole(Role role) noexcept { return role == Role::Host || role == Role::Guest; }
    }

    Peer::Peer(Config config) : config_(std::move(config)), transport_(Transport(config_))
    {
        if (!ValidRole(config_.role) || Empty(config_.profile) ||
            config_.transferTimeoutMilliseconds < 100 || config_.transferTimeoutMilliseconds > 600000)
            throw std::invalid_argument("Explicit bounded bootstrap role/profile/timeout required");
        if (Host() ? config_.transport.server.has_value() : !config_.transport.server.has_value())
            throw std::invalid_argument("Only Guest supplies bootstrap server endpoint");
    }
    Peer::~Peer() { transport_.Close(); }
    bool Peer::Fail(Terminal why) noexcept
    { if (terminal_ == Terminal::None) terminal_ = why; transport_.Close(); return false; }
    void Peer::Stop(Terminal why) noexcept
    {
        if (why == Terminal::None || why > Terminal::Internal) why = Terminal::LocalStop;
        (void)Fail(why);
    }
    bool Peer::Advance(std::uint64_t now)
    {
        if (terminal_ != Terminal::None) return false;
        if (hasTime_ && now < lastNow_) return Fail(Terminal::Clock);
        hasTime_ = true; lastNow_ = now; return true;
    }
    bool Peer::Start(std::uint64_t now)
    {
        if (started_ || !Advance(now)) return false;
        started_ = true;
        if (!transport_.Start(now) || (Host() && !transport_.HostStart(CarrierStartToken, now))) return Fail(Terminal::Transport);
        return true;
    }
    bool Peer::IsNext(const Assignment& a) const noexcept
    {
        if (!haveAssignment_) return true;
        if (assignment_.levelEpoch == UINT32_MAX || assignment_.generation == UINT32_MAX ||
            a.levelEpoch != assignment_.levelEpoch + 1 || a.generation != assignment_.generation + 1 ||
            a.sessionId == assignment_.sessionId || a.inputToken == assignment_.inputToken ||
            a.inputToken == assignment_.worldToken || a.worldToken == assignment_.inputToken || a.worldToken == assignment_.worldToken) return false;
        const std::array<std::uint64_t, 4> old{{assignment_.hostInputNonce, assignment_.hostWorldNonce, assignment_.guestInputNonce, assignment_.guestWorldNonce}};
        const std::array<std::uint64_t, 4> next{{a.hostInputNonce, a.hostWorldNonce, a.guestInputNonce, a.guestWorldNonce}};
        for (auto value : next) if (std::find(old.begin(), old.end(), value) != old.end()) return false;
        return true;
    }
    bool Peer::Publish(Assignment assignment, const std::vector<std::uint8_t>& bytes, std::uint64_t now)
    {
        if (!Advance(now) || !started_ || !Host()) return false;
        if (bytes.size() > MaximumBootstrapBytes || !ValidPayloadBytes(static_cast<std::uint32_t>(bytes.size())) || bytes.size() != SeedWitnessBytes + assignment.checkpointBytes ||
            assignment.controlPort != transport_.BoundEndpoint().port) return false;
        const auto digest = ComputeDigest(bytes.data(), bytes.size());
        if (!Empty(assignment.payloadDigest) && assignment.payloadDigest != digest) return Fail(Terminal::Digest);
        assignment.payloadDigest = digest;
        if (!ValidAssignment(assignment)) return false;
        if (haveAssignment_ && assignment.levelEpoch == assignment_.levelEpoch && assignment.generation == assignment_.generation)
            return (assignment == assignment_ && bytes == payload_) || Fail(Terminal::Sequence);
        if (haveAssignment_ && (!acknowledged_ || !IsNext(assignment))) return Fail(Terminal::Sequence);
        if (transferId_ == UINT64_MAX) return Fail(Terminal::Sequence);
        assignment_ = assignment; payload_ = bytes; ++transferId_; haveAssignment_ = true;
        offerSent_ = commitSent_ = acknowledged_ = false; sentOffset_ = 0; transferStarted_ = 0; transferStartedValid_ = false;
        localReady_ = remoteReady_ = readySent_ = false; localReadyReceipt_ = remoteReadyReceipt_ = {}; readyStarted_ = 0;
        readyStartedValid_ = false;
        outgoingNotices_.clear(); receivedNotices_.clear(); localNotices_ = {}; remoteNotices_ = {};
        lastProgress_ = now;
        return true;
    }
    bool Peer::SendBootstrap(const Message& value, std::uint64_t now)
    {
        const auto bytes = Encode(value);
        if (!bytes) return Fail(Terminal::Protocol);
        return transport_.SendApplicationMessage(connection_, ApplicationClass, static_cast<std::uint32_t>(value.kind),
            bytes->data(), static_cast<std::uint16_t>(bytes->size()), now);
    }
    bool Peer::SendPending(std::uint64_t now)
    {
        if (!helloSent_)
        {
            Message hello; hello.profile = config_.profile;
            if (!SendBootstrap(hello, now)) return terminal_ == Terminal::None;
            helloSent_ = true;
        }
        if (!profileMatched_) return true;
        for (std::size_t i = 0; i < n::WindowsTcpTransport::ApplicationQueueLimit; ++i)
        {
            if (!outgoingControl_.empty())
            {
                const auto& message = outgoingControl_.front();
                if (!transport_.SendApplicationMessage(connection_, message.classId, message.key, message.payload.data(),
                    static_cast<std::uint16_t>(message.payload.size()), now)) return true;
                outgoingControl_.pop_front(); continue;
            }
            if (acknowledgmentPending_)
            {
                Message ack; ack.kind = Kind::BytesAck; ack.transferId = transferId_;
                ack.totalBytes = static_cast<std::uint32_t>(payload_.size()); ack.digest = assignment_.payloadDigest;
                if (!SendBootstrap(ack, now)) return terminal_ == Terminal::None;
                acknowledgmentPending_ = false; continue;
            }
            if (!outgoingNotices_.empty())
            {
                Message notice; notice.kind = Kind::OwnerNotice; notice.notice = outgoingNotices_.front();
                if (!SendBootstrap(notice, now)) return terminal_ == Terminal::None;
                outgoingNotices_.pop_front(); continue;
            }
            if (localReady_ && !readySent_)
            {
                Message ready; ready.kind = Kind::OwnerReady; ready.ready = localReadyReceipt_;
                if (!SendBootstrap(ready, now)) return terminal_ == Terminal::None;
                readySent_ = true; continue;
            }
            if (!Host() || !haveAssignment_ || acknowledged_ || commitSent_) break;
            Message message; message.transferId = transferId_;
            message.totalBytes = static_cast<std::uint32_t>(payload_.size());
            if (!offerSent_)
            {
                message.kind = Kind::Offer; message.assignment = assignment_;
                if (!SendBootstrap(message, now)) return terminal_ == Terminal::None;
                offerSent_ = true; transferStarted_ = now; transferStartedValid_ = true; lastProgress_ = now; continue;
            }
            if (sentOffset_ < payload_.size())
            {
                message.kind = Kind::Chunk; message.offset = sentOffset_;
                const auto bytes = (std::min)(MaximumChunkBytes, static_cast<std::uint32_t>(payload_.size() - sentOffset_));
                message.chunk.assign(payload_.begin() + sentOffset_, payload_.begin() + sentOffset_ + bytes);
                if (!SendBootstrap(message, now)) return terminal_ == Terminal::None;
                sentOffset_ += bytes; continue;
            }
            message.kind = Kind::Commit; message.digest = assignment_.payloadDigest;
            if (!SendBootstrap(message, now)) return terminal_ == Terminal::None;
            commitSent_ = true;
        }
        return true;
    }
    std::optional<n::SessionMessage> Peer::ControlRecord(const void* bytes, std::size_t count)
    {
        if (!bytes || count != ControlBytes) return std::nullopt;
        n::SessionMessage value; value.classId = p::ApplicationClass;
        const auto* data = static_cast<const std::uint8_t*>(bytes); value.payload.assign(data, data + count);
        // The sole existing Progression decoder owns key/kind equality.
        for (std::uint32_t kind = 1; kind <= 6; ++kind)
        { value.key = kind; if (p::Decode(value)) return value; }
        return std::nullopt;
    }
    Admission Peer::SendControl(const void* bytes, std::size_t count, std::uint64_t now)
    {
        if (!Advance(now) || !started_ || !profileMatched_) return Admission::Failed;
        if (outgoingControl_.size() >= MaximumControlRecords) return Admission::Full;
        const auto record = ControlRecord(bytes, count);
        if (!record) { (void)Fail(Terminal::Protocol); return Admission::Failed; }
        outgoingControl_.push_back(*record); return Admission::Accepted;
    }
    bool Peer::ReceiveBootstrap(const Message& message, std::uint64_t now)
    {
        if (message.kind == Kind::Hello)
        {
            if (message.profile != config_.profile) return Fail(Terminal::Profile);
            profileMatched_ = true; lastProgress_ = now; return true;
        }
        if (!profileMatched_) return Fail(Terminal::Profile);
        if (message.kind == Kind::Stop) return Fail(message.terminal);
        if (message.kind == Kind::OwnerNotice)
        {
            const auto& notice = message.notice;
            if (!haveAssignment_ || notice.role == config_.role || notice.sessionId != assignment_.sessionId ||
                notice.oldEpoch != assignment_.levelEpoch || notice.oldGeneration != assignment_.generation) return Fail(Terminal::Sequence);
            auto& previous = remoteNotices_[static_cast<std::uint32_t>(notice.kind) - 1];
            if (previous)
            {
                if (notice.operationId == previous->operationId) return notice == *previous || Fail(Terminal::Sequence);
                if (notice.operationId < previous->operationId) return Fail(Terminal::Sequence);
            }
            if (receivedNotices_.size() >= MaximumControlRecords) return Fail(Terminal::Backpressure);
            receivedNotices_.push_back(notice); previous = notice; lastProgress_ = now; return true;
        }
        if (message.kind == Kind::OwnerReady)
        {
            const auto& ready = message.ready;
            if (!haveAssignment_ || (Host() ? !acknowledged_ : !receiveComplete_ || !consumed_) ||
                ready.role == config_.role || ready.sessionId != assignment_.sessionId ||
                ready.levelEpoch != assignment_.levelEpoch || ready.generation != assignment_.generation) return Fail(Terminal::Sequence);
            if (remoteReady_) return ready == remoteReadyReceipt_ || Fail(Terminal::Sequence);
            remoteReadyReceipt_ = ready; remoteReady_ = true; lastProgress_ = now; return true;
        }
        if (Host())
        {
            if (message.kind != Kind::BytesAck || !haveAssignment_ || !commitSent_ ||
                message.transferId != transferId_ || message.totalBytes != payload_.size() || message.digest != assignment_.payloadDigest)
                return Fail(Terminal::Sequence);
            if (!acknowledged_) { readyStarted_ = now; readyStartedValid_ = true; }
            acknowledged_ = true; lastProgress_ = now; return true;
        }
        if (message.kind == Kind::Offer)
        {
            if (haveAssignment_ && message.transferId == transferId_)
                return (message.assignment == assignment_ && message.totalBytes == payload_.size()) || Fail(Terminal::Sequence);
            if (message.assignment.controlPort != config_.transport.server->port ||
                message.transferId != transferId_ + 1 || (haveAssignment_ && (!receiveComplete_ || !consumed_ ||
                acknowledgmentPending_ || !IsNext(message.assignment)))) return Fail(Terminal::Sequence);
            assignment_ = message.assignment; payload_.assign(message.totalBytes, 0); transferId_ = message.transferId;
            haveAssignment_ = true; receiveComplete_ = consumed_ = false; receivedOffset_ = 0;
            localReady_ = remoteReady_ = readySent_ = false; localReadyReceipt_ = remoteReadyReceipt_ = {}; readyStarted_ = 0;
            readyStartedValid_ = false;
            outgoingNotices_.clear(); receivedNotices_.clear(); localNotices_ = {}; remoteNotices_ = {};
            transferStarted_ = now; transferStartedValid_ = true; lastProgress_ = now; return true;
        }
        if (!haveAssignment_ || message.transferId != transferId_ || message.totalBytes != payload_.size()) return Fail(Terminal::Sequence);
        if (message.kind == Kind::Chunk)
        {
            if (message.offset < receivedOffset_)
                return message.chunk.size() <= receivedOffset_ - message.offset &&
                    std::equal(message.chunk.begin(), message.chunk.end(), payload_.begin() + message.offset) ? true : Fail(Terminal::Sequence);
            if (receiveComplete_ || message.offset != receivedOffset_) return Fail(Terminal::Sequence);
            std::copy(message.chunk.begin(), message.chunk.end(), payload_.begin() + message.offset);
            receivedOffset_ += static_cast<std::uint32_t>(message.chunk.size()); lastProgress_ = now; return true;
        }
        if (message.kind != Kind::Commit || receivedOffset_ != payload_.size() || message.digest != assignment_.payloadDigest)
            return Fail(Terminal::Sequence);
        if (receiveComplete_) return true; // Exact completed commit cannot redeliver or ACK twice.
        if (ComputeDigest(payload_.data(), payload_.size()) != assignment_.payloadDigest) return Fail(Terminal::Digest);
        receiveComplete_ = true; acknowledgmentPending_ = true; readyStarted_ = now; readyStartedValid_ = true;
        lastProgress_ = now; return true;
    }
    bool Peer::Receive(const n::TcpReceivedMessage& value, std::uint64_t now)
    {
        if (value.connectionId != connection_) return Fail(Terminal::Sequence);
        if (value.message.classId == p::ApplicationClass)
        {
            if (!profileMatched_ || !p::Decode(value.message)) return Fail(Terminal::Protocol);
            if (receivedControl_.size() == MaximumControlRecords) return Fail(Terminal::Backpressure);
            std::array<std::uint8_t, ControlBytes> copy{};
            std::copy(value.message.payload.begin(), value.message.payload.end(), copy.begin());
            receivedControl_.push_back(copy); lastProgress_ = now; return true;
        }
        const auto message = Decode(value.message);
        return message ? ReceiveBootstrap(*message, now) : Fail(Terminal::Protocol);
    }
    bool Peer::Pump(std::uint64_t now)
    {
        if (!Advance(now) || !started_) return false;
        transport_.Pump(now);
        while (transport_.TakeSessionEvent()) {} // This owner consumes every bounded carrier event.
        const auto active = transport_.ActiveConnectionId();
        if (active)
        {
            if (connection_ && connection_ != active) return Fail(Terminal::Sequence);
            if (!connection_) { connection_ = active; handshakeStarted_ = now; lastProgress_ = now; }
        }
        const auto* core = transport_.Session();
        if (connection_)
            while (const auto message = transport_.TakeMessage()) if (!Receive(*message, now)) return false;
        if (connection_ && core && core->Phase() == n::SessionPhase::Running)
        {
            if (!SendPending(now)) return false;
        }
        if (transport_.State() == n::TcpTransportState::Closed || transport_.State() == n::TcpTransportState::Failed ||
            (connection_ && !active)) return Fail(Terminal::Transport);
        if (connection_ && !profileMatched_ && now - handshakeStarted_ >= config_.transferTimeoutMilliseconds)
            return Fail(Terminal::Timeout);
        if (haveAssignment_ && transferStartedValid_ && (Host() ? !acknowledged_ : !receiveComplete_) &&
            now - transferStarted_ >= config_.transferTimeoutMilliseconds) return Fail(Terminal::Timeout);
        if (haveAssignment_ && readyStartedValid_ && !BothReady() && now - readyStarted_ >= config_.transferTimeoutMilliseconds)
            return Fail(Terminal::Timeout);
        return true;
    }
    bool Peer::HasBootstrap() const noexcept
    { return terminal_ == Terminal::None && !Host() && receiveComplete_ && !consumed_; }
    bool Peer::ConsumeBootstrap() noexcept
    { if (!HasBootstrap()) return false; consumed_ = true; return true; }
    bool Peer::ObserveReady(const ReadyReceipt& ready, std::uint64_t now)
    {
        if (!Advance(now) || !profileMatched_ || !haveAssignment_ || !ValidReady(ready) ||
            ready.role != config_.role || ready.sessionId != assignment_.sessionId ||
            ready.levelEpoch != assignment_.levelEpoch || ready.generation != assignment_.generation ||
            (Host() ? !acknowledged_ : !receiveComplete_ || !consumed_)) return false;
        if (localReady_) return ready == localReadyReceipt_ || Fail(Terminal::Sequence);
        localReadyReceipt_ = ready; localReady_ = true; lastProgress_ = now; return true;
    }
    Admission Peer::SendNotice(const OwnerNotice& notice, std::uint64_t now)
    {
        if (!Advance(now) || !profileMatched_ || !haveAssignment_ || !ValidNotice(notice) ||
            notice.role != config_.role || notice.sessionId != assignment_.sessionId ||
            notice.oldEpoch != assignment_.levelEpoch || notice.oldGeneration != assignment_.generation) return Admission::Failed;
        auto& previous = localNotices_[static_cast<std::uint32_t>(notice.kind) - 1];
        if (previous)
        {
            if (notice.operationId == previous->operationId)
            {
                if (notice == *previous) return Admission::Accepted;
                (void)Fail(Terminal::Sequence); return Admission::Failed;
            }
            if (notice.operationId < previous->operationId) { (void)Fail(Terminal::Sequence); return Admission::Failed; }
        }
        if (outgoingNotices_.size() >= MaximumControlRecords) return Admission::Full;
        outgoingNotices_.push_back(notice); previous = notice; return Admission::Accepted;
    }
    const OwnerNotice& Peer::FrontNotice() const
    { if (receivedNotices_.empty()) throw std::logic_error("No received owner notice"); return receivedNotices_.front(); }
    void Peer::ConsumeNotice() noexcept { if (!receivedNotices_.empty()) receivedNotices_.pop_front(); }
    const std::array<std::uint8_t, ControlBytes>& Peer::FrontControl() const
    { if (receivedControl_.empty()) throw std::logic_error("No received bootstrap control record"); return receivedControl_.front(); }
    void Peer::ConsumeControl() noexcept { if (!receivedControl_.empty()) receivedControl_.pop_front(); }
    Status Peer::Snapshot() const noexcept
    {
        Status result; result.role = config_.role; result.terminal = terminal_;
        result.profileMatched = profileMatched_; result.bootstrapAvailable = HasBootstrap(); result.bootstrapAcknowledged = acknowledged_;
        result.bound = transport_.BoundEndpoint(); result.peer = transport_.PeerEndpoint().value_or(n::Endpoint{});
        result.connectionId = connection_; result.transferId = transferId_; result.totalBytes = payload_.size();
        result.receivedBytes = Host() ? sentOffset_ : receivedOffset_;
        result.controlQueued = static_cast<std::uint32_t>(receivedControl_.size());
        // Local records plus a pending transport-byte indicator. Zero is only
        // a local send-drain observation, never a remote receipt/native ACK.
        result.pendingOutbound = static_cast<std::uint32_t>(outgoingControl_.size() + outgoingNotices_.size()) +
            (transport_.PendingSendBytes() ? 1u : 0u) +
            static_cast<std::uint32_t>(transport_.Session() ? transport_.Session()->OutboundCount() : 0);
        result.ownedSockets = static_cast<std::uint32_t>(transport_.OwnedSocketCount());
        const auto& metrics = transport_.Metrics(); result.socketsOpened = metrics.socketsOpened; result.socketsClosed = metrics.socketsClosed;
        result.wsaStarts = metrics.wsaStarts; result.wsaCleanups = metrics.wsaCleanups;
        result.lastPumpMilliseconds = lastNow_; result.lastProgressMilliseconds = lastProgress_; result.assignment = assignment_;
        if (terminal_ != Terminal::None) result.phase = terminal_ == Terminal::LocalStop ? Phase::Closed : Phase::Failed;
        else if (!started_) result.phase = Phase::Idle;
        else if (!connection_) result.phase = Host() ? Phase::Listening : Phase::Connecting;
        else if (!profileMatched_) result.phase = Phase::Handshake;
        else if (!haveAssignment_) result.phase = Phase::ProfileMatched;
        else if (Host()) result.phase = acknowledged_ ? (BothReady() ? Phase::Running : Phase::BootstrapReady) : Phase::Transferring;
        else result.phase = receiveComplete_ ? (BothReady() ? Phase::Running : Phase::BootstrapReady) : Phase::Transferring;
        return result;
    }
}
