#include "SessionCore.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <array>
#include <new>
#include <stdexcept>
#include <utility>

namespace winxcoop::networking
{
    namespace
    {
        std::uint32_t Word(const std::uint8_t* bytes)
        { return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24); }
        void PutWord(std::uint8_t* bytes, std::uint32_t value)
        { for(unsigned n = 0; n < 4; ++n) bytes[n] = static_cast<std::uint8_t>(value >> (n * 8)); }
        bool ValidEndpoint(const Endpoint& value)
        { return value.address != 0 && value.address != 0xffffffffu && value.port != 0; }
    }
    SessionCore::SessionCore(SessionConfig config):config_(std::move(config)),
        localId_(config_.role == SessionRole::Host ? HostId : UnassignedId)
    {
        if((config_.role != SessionRole::Host && config_.role != SessionRole::Guest) ||
           !ValidEndpoint(config_.local) || !config_.heartbeatMilliseconds ||
           config_.heartbeatMilliseconds >= config_.timeoutMilliseconds)
            throw std::invalid_argument("Session requires explicit endpoints and heartbeat below timeout");
        if((config_.role == SessionRole::Guest && (!config_.server || !ValidEndpoint(*config_.server))) ||
           (config_.role == SessionRole::Host && config_.server))
            throw std::invalid_argument("Guest requires a server endpoint; Host binds its local endpoint");
    }
    bool SessionCore::IsConnected() const noexcept
    { return phase_ != SessionPhase::Idle && phase_ != SessionPhase::Closed && phase_ != SessionPhase::Failed; }
    bool SessionCore::AdvanceTime(const std::uint64_t now)
    {
        if(phase_ == SessionPhase::Closed || phase_ == SessionPhase::Failed) return false;
        if(hasTime_ && now < lastNow_) { Fail(SessionError::ClockMovedBackward); return false; }
        hasTime_ = true; lastNow_ = now;
        if(IsConnected() && (now - lastReceived_ > config_.timeoutMilliseconds ||
           (pendingPing_ && now - lastPingSent_ > config_.timeoutMilliseconds)))
        { Fail(SessionError::Timeout); return false; }
        return true;
    }
    void SessionCore::Fail(const SessionError error) noexcept
    {
        if(phase_ == SessionPhase::Closed || phase_ == SessionPhase::Failed) return;
        phase_ = SessionPhase::Failed; error_ = error; ready_ = false;
        outbound_.clear(); incoming_.clear(); pendingPing_.reset();
        (void)framer_.FailTransport();
    }
    bool SessionCore::Emit(const std::uint16_t type, const std::uint16_t destination,
        const void* data, const std::uint16_t size, const std::uint32_t field1, const std::uint32_t field2)
    {
        if(outbound_.size() >= MaximumOutboundFrames) { Fail(SessionError::Backpressure); return false; }
        if(size > sparkplug::reconstruction::spNetworkPacket::PayloadCapacity || (size && !data))
            throw std::invalid_argument("Session payload exceeds its codec contract");
        try
        {
            sparkplug::reconstruction::spNetworkPacket packet(nullptr);
            if(!packet.FillForAnalysis(localId_, destination, type, 1, field1, field2, size, data))
                throw std::logic_error("Bounded Session Fill failed");
            sparkplug::reconstruction::spMemoryStream stream;
            const auto wireSize = static_cast<std::uint32_t>(TcpPacketFramer::HeaderSize + size);
            if(!stream.ResizeAndSetSize(wireSize) || !stream.GetBuffer())
            { Fail(SessionError::ResourceFailure); return false; }
            if(!stream.Reset() || !packet.WriteForAnalysis(stream))
                throw std::runtime_error("Session codec write failed");
            std::uint32_t actual = 0;
            if(!stream.GetSize(&actual) || actual != wireSize) throw std::logic_error("Session wire size mismatch");
            const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
            outbound_.emplace_back(bytes, bytes + actual); return true;
        }
        catch(const std::bad_alloc&) { Fail(SessionError::ResourceFailure); }
        catch(const std::exception&) { Fail(SessionError::CodecFailure); }
        return false;
    }
    bool SessionCore::Attach(const std::uint64_t now)
    {
        if(phase_ != SessionPhase::Idle || !AdvanceTime(now)) return false;
        lastReceived_ = lastPingSent_ = now;
        if(config_.role == SessionRole::Host) { phase_ = SessionPhase::AwaitingProbe; return true; }
        phase_ = SessionPhase::AwaitingInfo;
        constexpr std::uint8_t probe = 0x2a;
        return Emit(0, HostId, &probe, 1);
    }
    bool SessionCore::EmitStart()
    {
        if(!startToken_ || !ready_) return false;
        const std::array<std::uint8_t,4> data{1,0,static_cast<std::uint8_t>(*startToken_),static_cast<std::uint8_t>(*startToken_ >> 8)};
        if(!Emit(4, GuestId, data.data(), static_cast<std::uint16_t>(data.size()))) return false;
        phase_ = SessionPhase::Running; return true;
    }
    bool SessionCore::HostStart(const std::uint16_t token, const std::uint64_t now)
    {
        // Rejected guest commands do not alter the connection or emit traffic.
        if(config_.role != SessionRole::Host || !token || !AdvanceTime(now)) return false;
        if(startToken_) return *startToken_ == token;
        startToken_ = token;
        return phase_ == SessionPhase::WaitingForHost ? EmitStart() : true;
    }
    void SessionCore::Handle(const sparkplug::reconstruction::spNetworkPacket& packet, const std::uint64_t now)
    {
        const auto& header = packet.HeaderForAnalysis();
        if(!header.packetType || !header.useTcp || !*header.useTcp || header.size > TcpPacketFramer::MaximumPayloadSize)
        { Fail(SessionError::InvalidPacket); return; }
        const auto type = *header.packetType;
        const bool host = config_.role == SessionRole::Host;
        const auto expectedSource = host ? (type == 0 ? UnassignedId : GuestId) : HostId;
        const auto expectedDestination = !host && type == 1 ? 0xffff : localId_;
        if(header.source != expectedSource || header.destination != expectedDestination)
        { Fail(SessionError::InvalidIdentity); return; }
        if(type != 2 && (header.dataField1 || header.dataField2))
        { Fail(SessionError::InvalidPacket); return; }
        const auto* data = packet.KnownPayloadForAnalysis(header.size);
        switch(type)
        {
        case 0:
            if(!host || phase_ != SessionPhase::AwaitingProbe) { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 1 || data[0] != 0x2a) { Fail(SessionError::InvalidPacket); break; }
            { constexpr std::array<std::uint8_t,2> id{1,0};
              if(Emit(1,0xffff,id.data(),2)) phase_ = SessionPhase::AwaitingReady; }
            break;
        case 1:
            if(host || phase_ != SessionPhase::AwaitingInfo) { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 2) { Fail(SessionError::InvalidPacket); break; }
            if(std::uint16_t(data[0] | (std::uint16_t(data[1]) << 8)) != GuestId)
            { Fail(SessionError::InvalidAssignment); break; }
            { localId_ = GuestId; constexpr std::uint8_t ready = 1;
              if(Emit(3,HostId,&ready,1)) { ready_ = true; phase_ = SessionPhase::WaitingForHost; } }
            break;
        case 3:
            if(!host || phase_ != SessionPhase::AwaitingReady) { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 1 || data[0] != 1) { Fail(SessionError::InvalidPacket); break; }
            ready_ = true; phase_ = SessionPhase::WaitingForHost;
            if(startToken_) (void)EmitStart();
            break;
        case 4:
            if(host || phase_ != SessionPhase::WaitingForHost) { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 4 || data[0] != 1 || data[1] != 0) { Fail(SessionError::InvalidPacket); break; }
            { const auto token = std::uint16_t(data[2] | (std::uint16_t(data[3]) << 8));
              if(!token) { Fail(SessionError::InvalidAssignment); break; }
              startToken_ = token; phase_ = SessionPhase::Running; }
            break;
        case 2:
            if(phase_ != SessionPhase::Running) { Fail(SessionError::UnexpectedPacket); break; }
            if(incoming_.size() >= MaximumInboundMessages) { Fail(SessionError::Backpressure); break; }
            incoming_.push_back({header.dataField1,header.dataField2,{data,data + header.size}});
            break;
        case 5:
            if(!host || (phase_ != SessionPhase::WaitingForHost && phase_ != SessionPhase::Running))
            { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 8 || Word(data + 4) != 0) { Fail(SessionError::InvalidPacket); break; }
            { std::array<std::uint8_t,8> reply{}; PutWord(reply.data(),Word(data)); PutWord(reply.data()+4,static_cast<std::uint32_t>(now));
              (void)Emit(6,GuestId,reply.data(),8); }
            break;
        case 6:
            if(host || (phase_ != SessionPhase::WaitingForHost && phase_ != SessionPhase::Running))
            { Fail(SessionError::UnexpectedPacket); break; }
            if(header.size != 8 || !pendingPing_ || Word(data) != *pendingPing_)
            { Fail(SessionError::InvalidPacket); break; }
            // Our liveness clock is monotonic64. Timestamp words preserve the
            // native layout; no native latency formula or zero-word rule used.
            pendingPing_.reset();
            break;
        default: Fail(SessionError::UnexpectedPacket); break;
        }
        if(IsConnected()) lastReceived_ = now;
    }
    std::size_t SessionCore::Receive(const std::uint8_t* data, const std::size_t size, const std::uint64_t now)
    {
        if(size && !data) throw std::invalid_argument("Session input pointer is null");
        if(!IsConnected() || !AdvanceTime(now)) return 0;
        std::size_t consumed = 0;
        while(consumed < size && IsConnected())
        {
            auto result = framer_.Feed(data + consumed, size - consumed);
            consumed += result.consumed;
            if(result.state == StreamState::Failed) { Fail(SessionError::FramingFailure); break; }
            if(result.packet)
            {
                try { Handle(*result.packet,now); }
                catch(const std::bad_alloc&) { Fail(SessionError::ResourceFailure); }
                catch(const std::exception&) { Fail(SessionError::CodecFailure); }
            }
            else break;
        }
        return consumed;
    }
    void SessionCore::Tick(const std::uint64_t now)
    {
        if(!IsConnected() || !AdvanceTime(now)) return;
        if(config_.role == SessionRole::Guest && !pendingPing_ &&
           (phase_ == SessionPhase::WaitingForHost || phase_ == SessionPhase::Running) &&
           now - lastPingSent_ >= config_.heartbeatMilliseconds)
        {
            std::array<std::uint8_t,8> data{}; const auto stamp = static_cast<std::uint32_t>(now);
            PutWord(data.data(),stamp);
            if(Emit(5,HostId,data.data(),8)) { pendingPing_ = stamp; lastPingSent_ = now; }
        }
    }
    void SessionCore::EndOfInput(const std::uint64_t now)
    {
        if(!IsConnected() || !AdvanceTime(now)) return;
        if(framer_.EndOfInput().state == StreamState::Failed) Fail(SessionError::FramingFailure);
        else Close();
    }
    void SessionCore::FailTransport() noexcept { Fail(SessionError::TransportFailure); }
    void SessionCore::Close() noexcept
    {
        if(phase_ == SessionPhase::Failed) return;
        phase_ = SessionPhase::Closed; ready_ = false;
        outbound_.clear(); incoming_.clear(); pendingPing_.reset(); framer_.Reset();
    }
    bool SessionCore::SendApplicationMessage(const std::uint32_t classId, const std::uint32_t key,
        const void* data, const std::uint16_t size, const std::uint64_t now)
    {
        if(size > TcpPacketFramer::MaximumPayloadSize || (size && !data))
            throw std::invalid_argument("Session message pointer/size is invalid");
        if(phase_ != SessionPhase::Running || !AdvanceTime(now)) return false;
        return Emit(2,config_.role == SessionRole::Host ? GuestId : HostId,data,size,classId,key);
    }
    std::optional<std::vector<std::uint8_t>> SessionCore::TakeOutbound()
    {
        if(outbound_.empty()) return std::nullopt;
        auto frame = std::move(outbound_.front()); outbound_.pop_front(); return frame;
    }
    std::optional<SessionMessage> SessionCore::TakeMessage()
    {
        if(incoming_.empty()) return std::nullopt;
        auto message = std::move(incoming_.front()); incoming_.pop_front(); return message;
    }
}
