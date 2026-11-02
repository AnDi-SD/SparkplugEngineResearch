#include "WorldStateSession.h"
#include <algorithm>
#include <stdexcept>

namespace winxcoop::world_state
{
    namespace
    {
        bool EntrySeedMatchesIdentity(const Snapshot& snapshot,const SessionIdentity& identity)
        {
            for(const auto& block:snapshot.blocks)
                if(block.type==BlockType::EntrySeedV1)
                {
                    auto seed=DecodeEntrySeed(block.data.data(),block.data.size());
                    if(!seed || seed->levelEpoch!=identity.levelEpoch || seed->generation!=identity.generation) return false;
                }
            // Generic transport fixtures may omit entry data. A configured
            // native consumer separately requires its initial entry decision.
            return true;
        }
    }
    Session::Session(Role role,SessionIdentity id):role_(role),id_(id),token_(role==Role::Host?id.hostToken:0)
    {
        if((role!=Role::Host&&role!=Role::Guest)||!id.sessionId||!id.scenarioId||!id.levelEpoch||!id.generation||!id.hostToken)
            throw std::invalid_argument("Explicit WorldState identity required");
    }
    Message Session::Base(Kind kind) const
    {
        Message m; m.kind=kind;m.sessionId=id_.sessionId;m.scenarioId=id_.scenarioId;m.connectionToken=token_;
        m.levelEpoch=id_.levelEpoch;m.generation=id_.generation;m.syncEpoch=epoch_;return m;
    }
    bool Session::Fail(Terminal why) noexcept { Stop(why); return false; }
    void Session::Stop(Terminal why) noexcept
    {
        if(terminal_!=Terminal::None) return;
        terminal_=why==Terminal::None?Terminal::InvalidRecord:why;phase_=Phase::Closed;
        const auto sent=sent_,applied=applied_,received=received_;
        controls_.clear();ClearEpoch();latest_.clear();
        // Preserve final observation counters for the terminal report; Closed
        // and cleared epoch state still forbid any further apply/publication.
        sent_=sent;applied_=applied;received_=received;
    }
    void Session::ClearEpoch() noexcept
    {
        handshake_=false;epochActive_=false;waitingNative_=false;receivedTaken_=false;
        flight_.clear();flightOffset_=0;flightRevision_=0;assembly_.Clear();delivery_.reset();
        sent_=applied_=received_=0;
    }
    bool Session::Queue(Message m)
    {
        if(!ValidMessage(m)) return Fail(Terminal::InvalidRecord);
        if(controls_.size()>=4) return Fail(Terminal::Sequence);
        controls_.push_back(std::move(m));return true;
    }
    bool Session::BeginEpoch()
    {
        if(role_!=Role::Host||!localReady_||!peerReady_||!hello_||epochActive_) return true;
        if(epoch_==UINT32_MAX) return Fail(Terminal::CounterExhausted);
        ClearEpoch();++epoch_;epochActive_=true;phase_=Phase::Connected;
        auto m=Base(Kind::Ready);m.revision=localRevision_;m.flags=1;return Queue(std::move(m));
    }
    bool Session::ObserveActor(Availability availability,std::uint32_t revision)
    {
        if(terminal_!=Terminal::None) return false;
        if(availability==Availability::Retired) return Fail(Terminal::ActorClosed);
        if((availability!=Availability::Waiting&&availability!=Availability::Ready)||!revision||revision<localRevision_)
            return Fail(Terminal::Identity);
        const bool ready=availability==Availability::Ready;
        if(revision==localRevision_) return ready==localReady_ || Fail(Terminal::Sequence);
        const bool wasReady=localReady_;localRevision_=revision;localReady_=ready;
        if(role_==Role::Guest)
        {
            if(!helloSent_)
            { controls_.clear();auto m=Base(Kind::Hello);m.connectionToken=0;m.syncEpoch=0;m.revision=revision;m.flags=ready?1u:0u;return Queue(std::move(m)); }
            // No readiness control may overtake an old uncommitted ACK.
            controls_.clear();ClearEpoch();phase_=Phase::Connected;
            auto m=Base(Kind::SourceState);m.revision=revision;m.flags=ready?1u:0u;
            return Queue(std::move(m));
        }
        if(wasReady || epochActive_)
        {
            controls_.clear();ClearEpoch();phase_=Phase::Connected;
            if(epoch_) { auto m=Base(Kind::Pause);if(!Queue(std::move(m))) return false; }
        }
        return BeginEpoch();
    }
    bool Session::BeginSnapshot()
    {
        if(role_!=Role::Host||!handshake_||!epochActive_||!localReady_||!peerReady_||!flight_.empty()||sent_!=applied_||latest_.empty()||latestRevision_<=sent_) return true;
        flight_=latest_;flightRevision_=latestRevision_;flightOffset_=0;return true;
    }
    bool Session::OfferSnapshot(const Snapshot& snapshot)
    {
        if(terminal_!=Terminal::None||role_!=Role::Host) return false;
        auto bytes=EncodeSnapshot(snapshot);if(!bytes) return Fail(Terminal::InvalidRecord);
        if(!EntrySeedMatchesIdentity(snapshot,id_)) return Fail(Terminal::Identity);
        if(snapshot.revision<latestRevision_||snapshot.nativeTick<latestTick_) return Fail(Terminal::Sequence);
        if(snapshot.revision==latestRevision_)
            return *bytes==latest_ || Fail(Terminal::Sequence);
        if(!latest_.empty()&&latestRevision_>sent_&&latestRevision_!=flightRevision_) ++coalesced_;
        latest_=std::move(*bytes);latestRevision_=snapshot.revision;latestTick_=snapshot.nativeTick;
        return BeginSnapshot();
    }
    bool Session::ObserveApplied(std::uint32_t epoch,std::uint64_t revision)
    {
        if(terminal_!=Terminal::None||role_!=Role::Guest) return false;
        if(!revision) return epoch==0 || Fail(Terminal::Sequence);
        if(epoch<epoch_) return true; // Previous native fence remains visible until the Actor observes new epoch.
        if(epoch!=epoch_) return Fail(Terminal::Sequence);
        if(!epochActive_) return true; // Cached ACK from the just-paused epoch is not a new native apply.
        if(revision==applied_) return true;
        if(!epochActive_||!localReady_||!waitingNative_||!receivedTaken_||revision!=received_) return Fail(Terminal::NativeApply);
        if(!controls_.empty()&&controls_.back().kind==Kind::AppliedAck)
            return controls_.back().revision==revision || Fail(Terminal::Sequence);
        auto m=Base(Kind::AppliedAck);m.revision=revision;return Queue(std::move(m));
    }
    bool Session::Receive(const Message& m)
    {
        if(terminal_!=Terminal::None) return false;
        if(!ValidMessage(m)||m.sessionId!=id_.sessionId||m.scenarioId!=id_.scenarioId||m.levelEpoch!=id_.levelEpoch||m.generation!=id_.generation)
            return Fail(Terminal::Identity);
        if(role_==Role::Host && m.kind==Kind::Hello)
        {
            if(hello_||m.revision>UINT32_MAX) return Fail(Terminal::Sequence);
            hello_=true;peerReady_=m.flags==1;peerRevision_=static_cast<std::uint32_t>(m.revision);phase_=Phase::Connected;return BeginEpoch();
        }
        if(role_==Role::Guest && m.kind==Kind::Ready)
        {
            if(!helloSent_||m.connectionToken!=id_.hostToken||m.revision>UINT32_MAX||m.revision<peerRevision_||m.syncEpoch<=epoch_)
                return Fail(Terminal::Sequence);
            token_=m.connectionToken;peerRevision_=static_cast<std::uint32_t>(m.revision);peerReady_=true;
            controls_.clear();ClearEpoch();epoch_=m.syncEpoch;epochActive_=true;phase_=Phase::Connected;
            if(!localReady_) return true;
            auto response=Base(Kind::Ready);response.revision=localRevision_;response.flags=1;return Queue(std::move(response));
        }
        if(!token_||m.connectionToken!=token_) return Fail(Terminal::Identity);
        if(m.kind==Kind::Stop) return Fail(static_cast<Terminal>(m.terminalReason));
        if(role_==Role::Host)
        {
            if(!hello_) return Fail(Terminal::Sequence);
            if(m.kind==Kind::SourceState)
            {
                if(m.revision<=peerRevision_||m.revision>UINT32_MAX) return Fail(Terminal::Sequence);
                peerRevision_=static_cast<std::uint32_t>(m.revision);peerReady_=m.flags==1;
                controls_.clear();ClearEpoch();phase_=Phase::Connected;
                if(epoch_) {auto pause=Base(Kind::Pause);if(!Queue(std::move(pause))) return false;}
                return BeginEpoch();
            }
            if(m.syncEpoch!=epoch_||!epochActive_) return Fail(Terminal::Sequence);
            if(m.kind==Kind::Ready)
            {
                if(handshake_||!localReady_||!peerReady_||m.revision!=peerRevision_) return Fail(Terminal::Sequence);
                handshake_=true;return BeginSnapshot();
            }
            if(m.kind==Kind::AppliedAck)
            {
                if(!handshake_||!sent_||sent_==applied_||m.revision!=sent_||!flight_.empty()) return Fail(Terminal::Sequence);
                if(appliedCount_==UINT64_MAX)return Fail(Terminal::CounterExhausted);
                applied_=m.revision;++appliedCount_;phase_=Phase::Ready;return BeginSnapshot();
            }
            return Fail(Terminal::Sequence);
        }
        if(m.kind==Kind::Pause)
        {
            if(m.syncEpoch<epoch_) return true;
            if(m.syncEpoch>epoch_) epoch_=m.syncEpoch;
            controls_.clear();ClearEpoch();phase_=Phase::Connected;return true;
        }
        if(m.kind!=Kind::Snapshot||m.syncEpoch!=epoch_||!epochActive_||!handshake_||!localReady_||waitingNative_||m.revision<=received_)
            return Fail(Terminal::Sequence);
        if(!assembly_.Push(m)) return Fail(Terminal::Sequence);
        if(assembly_.Size()==m.totalBytes)
        {
            auto snapshot=assembly_.Complete();if(!snapshot) return Fail(Terminal::InvalidRecord);
            if(!EntrySeedMatchesIdentity(*snapshot,id_)) return Fail(Terminal::Identity);
            received_=snapshot->revision;delivery_=std::move(snapshot);waitingNative_=true;receivedTaken_=false;
        }
        return true;
    }
    std::optional<Message> Session::PeekOutbound() const
    {
        if(terminal_!=Terminal::None) return std::nullopt;
        if(!controls_.empty()) return controls_.front();
        if(role_!=Role::Host||flight_.empty()) return std::nullopt;
        auto m=Base(Kind::Snapshot);m.revision=flightRevision_;m.totalBytes=static_cast<std::uint32_t>(flight_.size());m.offset=flightOffset_;
        const auto count=(std::min)(MaximumChunkBytes,m.totalBytes-m.offset);
        m.chunk.assign(flight_.begin()+m.offset,flight_.begin()+m.offset+count);return m;
    }
    bool Session::CommitOutbound()
    {
        if(terminal_!=Terminal::None) return false;
        if(!controls_.empty())
        {
            const auto m=controls_.front();controls_.pop_front();
            if(role_==Role::Guest&&m.kind==Kind::Hello) {helloSent_=true;token_=id_.hostToken;}
            if(role_==Role::Guest&&m.kind==Kind::Ready) handshake_=true;
            if(role_==Role::Guest&&m.kind==Kind::AppliedAck)
            { if(appliedCount_==UINT64_MAX)return Fail(Terminal::CounterExhausted);applied_=m.revision;++appliedCount_;waitingNative_=false;assembly_.Clear();phase_=Phase::Ready; }
            return true;
        }
        if(role_!=Role::Host||flight_.empty()) return Fail(Terminal::Sequence);
        flightOffset_+=(std::min)(MaximumChunkBytes,static_cast<std::uint32_t>(flight_.size())-flightOffset_);
        if(flightOffset_==flight_.size()) {sent_=flightRevision_;flight_.clear();flightOffset_=0;flightRevision_=0;}
        return true;
    }
    std::optional<Snapshot> Session::TakeReceivedSnapshot()
    {
        if(!delivery_) return std::nullopt;
        receivedTaken_=true;auto value=std::move(delivery_);delivery_.reset();return value;
    }
}
