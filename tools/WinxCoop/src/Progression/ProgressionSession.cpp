#include "ProgressionSession.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace winxcoop::progression
{
    namespace
    {
        bool Nonzero(const Digest& value) noexcept
        { return std::any_of(value.begin(),value.end(),[](auto byte){return byte!=0;}); }
        bool CompleteRetirement(const NativeRetirementReceipt& value) noexcept
        {
            return value.originalExitReturned&&value.inputRetired&&value.worldRetired&&value.bindingsRetired&&
                value.borrowedBaselinesRestored&&value.patchesRestored&&value.noNativeOwnership&&
                value.noPendingNativeCallsOrApply&&value.laterWorldResourcesReleased;
        }
        bool SameEntry(const NativeEntryReceipt& a,const NativeEntryReceipt& b) noexcept
        {
            return a.scene==b.scene&&a.descriptor==b.descriptor&&a.channels==b.channels&&a.seed==b.seed&&
                a.witness==b.witness&&a.localEntryOrdinal==b.localEntryOrdinal&&
                a.originalLoadReturned==b.originalLoadReturned&&a.actualReadyNativeWorld==b.actualReadyNativeWorld;
        }
        bool Direction(Role receiver,Kind kind) noexcept
        {
            return kind==Kind::Abort || (receiver==Role::Host ? kind==Kind::Retired||kind==Kind::Loaded :
                kind==Kind::Prepare||kind==Kind::Load||kind==Kind::Resume);
        }
        Message WithKind(Message value,Kind kind) { value.kind=kind;return value; }
    }
    Session::Session(Role role,ConnectionIdentity connection,TrustedInitialRunning initial,std::uint64_t timeout)
        :role_(role),connection_(connection),current_(initial),timeout_(timeout)
    {
        if((role!=Role::Host&&role!=Role::Guest)||!connection.sessionId||!connection.scenarioId||!connection.token||
            !ValidScene(initial.scene)||!ValidDescriptor(initial.descriptor)||!ValidChannels(initial.channels)||
            timeout<100||timeout>600000)
            throw std::invalid_argument("Explicit progression role, connection, trusted initial scene and bounded timeout required");
    }
    void Session::Stop(Terminal why) noexcept
    {
        if(terminal_!=Terminal::None)return;
        terminal_=why==Terminal::None||why>Terminal::Disconnected?Terminal::Disconnected:why;
        phase_=Phase::Closed;outbound_.clear();deadline_=0;
    }
    bool Session::Fail(Terminal why) noexcept { Stop(why);return false; }
    bool Session::Advance(std::uint64_t now)
    {
        if(terminal_!=Terminal::None)return false;
        if(hasTime_&&now<lastNow_)return Fail(Terminal::Clock);
        hasTime_=true;lastNow_=now;
        if(deadline_&&now>=deadline_)return Fail(Terminal::Timeout);
        return true;
    }
    bool Session::Poll(std::uint64_t now) { return Advance(now); }
    Message Session::Base(Kind kind) const
    {
        Message value;value.kind=kind;value.connection=connection_;
        if(decision_)value.decision=*decision_;
        return value;
    }
    bool Session::Queue(const Message& value)
    {
        if(!ValidMessage(value))return Fail(Terminal::InvalidRecord);
        if(outbound_.size()>=MaximumOutboundMessages)return Fail(Terminal::Backpressure);
        outbound_.push_back(value);return true;
    }
    bool Session::FreshChannels(const ChannelGeneration& value) const noexcept
    {
        if(!ValidChannels(value)||value.sessionId==current_.channels.sessionId||
            value.inputToken==current_.channels.inputToken||value.inputToken==current_.channels.worldToken||
            value.worldToken==current_.channels.inputToken||value.worldToken==current_.channels.worldToken)return false;
        const std::array<std::uint64_t,4> previous={current_.channels.hostInputNonce,current_.channels.hostWorldNonce,
            current_.channels.guestInputNonce,current_.channels.guestWorldNonce};
        const std::array<std::uint64_t,4> next={value.hostInputNonce,value.hostWorldNonce,value.guestInputNonce,value.guestWorldNonce};
        for(auto nonce:next)if(std::find(previous.begin(),previous.end(),nonce)!=previous.end())return false;
        return true;
    }
    bool Session::Start(const Decision& decision,std::uint64_t now)
    {
        if(!ValidDecision(decision)||!(decision.previous==current_.scene)||decision.operation<=completed_)
            return Fail(Terminal::Identity);
        if(now>(std::numeric_limits<std::uint64_t>::max)()-timeout_)return Fail(Terminal::CounterExhausted);
        if(!outbound_.empty())return Fail(Terminal::Sequence);
        decision_=decision;deadline_=now+timeout_;
        prepareSent_=localRetired_=peerRetired_=retiredSent_=hostEntry_=loadSent_=loadReceived_=loadTaken_=loadedSent_=false;
        load_.reset();loaded_.reset();localEntry_.reset();phase_=Phase::Preparing;
        return true;
    }
    bool Session::Begin(const Decision& decision,std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(role_!=Role::Host)return Fail(Terminal::Authority);
        if(!ValidDecision(decision))return Fail(Terminal::InvalidRecord);
        if(decision_&&decision.operation==decision_->operation)
            return decision==*decision_ || Fail(Terminal::Identity);
        if(decision.operation<=completed_)return true; // Stale local request never starts native work again.
        if(phase_!=Phase::Running)return Fail(Terminal::Sequence);
        return Start(decision,now)&&Queue(Base(Kind::Prepare));
    }
    bool Session::MatchDecision(const Decision& decision)
    { return decision_&&decision==*decision_ || Fail(Terminal::Identity); }
    bool Session::ObserveLocalRetired(const NativeRetirementReceipt& receipt,std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(!decision_||phase_==Phase::Running||!(receipt.scene==decision_->previous)||!CompleteRetirement(receipt))
            return Fail(Terminal::NativeReceipt);
        if(localRetired_)return true;
        localRetired_=true;
        if(role_==Role::Host)return MaybeLoad();
        auto value=Base(Kind::Retired);value.retirementMask=CompleteRetirementMask;
        phase_=Phase::WaitingLoad;return Queue(value);
    }
    bool Session::ObserveHostEntry(const NativeEntryReceipt& receipt,std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(role_!=Role::Host)return Fail(Terminal::Authority);
        if(!decision_||phase_==Phase::Running||!localRetired_||!(receipt.scene==decision_->next)||
            !(receipt.descriptor==decision_->destination)||!FreshChannels(receipt.channels)||
            !receipt.localEntryOrdinal||!receipt.originalLoadReturned||!receipt.actualReadyNativeWorld||
            !Nonzero(receipt.seed)||!Nonzero(receipt.witness))return Fail(Terminal::NativeReceipt);
        if(localEntry_)return SameEntry(receipt,*localEntry_)||Fail(Terminal::NativeReceipt);
        localEntry_=receipt;hostEntry_=true;
        auto value=Base(Kind::Load);value.channels=receipt.channels;value.seed=receipt.seed;value.witness=receipt.witness;
        load_=value;return MaybeLoad();
    }
    bool Session::MaybeLoad()
    {
        if(role_!=Role::Host||!localRetired_||!peerRetired_||!hostEntry_||loadSent_)return true;
        if(std::any_of(outbound_.begin(),outbound_.end(),[](const auto& value){return value.kind==Kind::Load;}))return true;
        if(!prepareSent_||!load_)return Fail(Terminal::Sequence);
        return Queue(*load_);
    }
    std::optional<Message> Session::TakeLoadAuthorization(std::uint64_t now)
    {
        if(!Advance(now))return std::nullopt;
        if(role_!=Role::Guest){(void)Fail(Terminal::Authority);return std::nullopt;}
        if(!loadReceived_||loadTaken_||!load_||phase_!=Phase::Loading)return std::nullopt;
        loadTaken_=true;return load_;
    }
    bool Session::ObserveGuestEntryApplied(const NativeEntryReceipt& entry,const NativeWorldAppliedReceipt& applied,std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(role_!=Role::Guest)return Fail(Terminal::Authority);
        if(!decision_||!load_||!loadReceived_||!loadTaken_||phase_==Phase::Running||
            !(entry.scene==decision_->next)||!(entry.descriptor==decision_->destination)||
            !(entry.channels==load_->channels)||entry.seed!=load_->seed||entry.witness!=load_->witness||
            !entry.localEntryOrdinal||!entry.originalLoadReturned||!entry.actualReadyNativeWorld||
            !(applied.scene==decision_->next)||!(applied.channels==load_->channels)||
            !applied.revision||!applied.nativeTick||!applied.completeNativeSnapshotApplied||!applied.freshNeutralInputBoundaryObserved)
            return Fail(Terminal::NativeReceipt);
        auto value=WithKind(*load_,Kind::Loaded);value.appliedRevision=applied.revision;value.nativeTick=applied.nativeTick;
        if(loaded_)return *loaded_==value&&localEntry_&&SameEntry(entry,*localEntry_)||Fail(Terminal::NativeReceipt);
        localEntry_=entry;loaded_=value;phase_=Phase::WaitingResume;return Queue(value);
    }
    bool Session::Receive(const Message& message,std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(!ValidMessage(message))return Fail(Terminal::InvalidRecord);
        if(!(message.connection==connection_))return Fail(Terminal::Identity);
        if(!Direction(role_,message.kind))return Fail(Terminal::Authority);
        if(message.decision.operation<=completed_&&(!decision_||message.decision.operation!=decision_->operation))return true;
        if(decision_&&message.decision.operation==decision_->operation&&!(message.decision==*decision_))return Fail(Terminal::Identity);
        if(message.kind==Kind::Prepare)
        {
            if(decision_&&message.decision.operation==decision_->operation)return true;
            if(message.decision.operation<=completed_)return true;
            if(phase_!=Phase::Running)return Fail(Terminal::Sequence);
            return Start(message.decision,now);
        }
        if(!MatchDecision(message.decision))return false;
        if(message.kind==Kind::Abort)return Fail(message.terminal);
        if(message.kind==Kind::Retired)
        {
            if(completed_==message.decision.operation&&phase_==Phase::Running)return true;
            if(!prepareSent_)return Fail(Terminal::Sequence);
            peerRetired_=true;return MaybeLoad();
        }
        if(message.kind==Kind::Load)
        {
            if(load_)return *load_==message||Fail(Terminal::Identity);
            if(!localRetired_||!retiredSent_||!FreshChannels(message.channels)||phase_!=Phase::WaitingLoad)
                return Fail(Terminal::Sequence);
            load_=message;loadReceived_=true;phase_=Phase::Loading;return true;
        }
        if(message.kind==Kind::Loaded)
        {
            if(loaded_)return *loaded_==message||Fail(Terminal::Identity);
            if(!loadSent_||!load_||!(message.channels==load_->channels)||message.seed!=load_->seed||message.witness!=load_->witness||
                phase_!=Phase::WaitingGuest)return Fail(Terminal::Sequence);
            loaded_=message;phase_=Phase::ResumePending;return Queue(WithKind(message,Kind::Resume));
        }
        if(message.kind==Kind::Resume)
        {
            if(!loaded_||!loadedSent_||!(WithKind(*loaded_,Kind::Resume)==message))return Fail(Terminal::Sequence);
            if(completed_==message.decision.operation&&phase_==Phase::Running)return true;
            if(phase_!=Phase::WaitingResume)return Fail(Terminal::Sequence);
            Complete();return true;
        }
        return Fail(Terminal::Sequence);
    }
    std::optional<Message> Session::PeekOutbound() const
    { return terminal_==Terminal::None&&!outbound_.empty()?std::optional<Message>(outbound_.front()):std::nullopt; }
    bool Session::CommitOutbound(std::uint64_t now)
    {
        if(!Advance(now))return false;
        if(outbound_.empty())return Fail(Terminal::Sequence);
        const auto message=outbound_.front();outbound_.pop_front();
        switch(message.kind)
        {
        case Kind::Prepare:prepareSent_=true;return true;
        case Kind::Retired:retiredSent_=true;return true;
        case Kind::Load:loadSent_=true;phase_=Phase::WaitingGuest;return true;
        case Kind::Loaded:loadedSent_=true;return true;
        case Kind::Resume:Complete();return true;
        default:return Fail(Terminal::Sequence);
        }
    }
    void Session::Complete() noexcept
    {
        current_={decision_->next,decision_->destination,load_->channels};
        completed_=decision_->operation;phase_=Phase::Running;deadline_=0;
    }
}
