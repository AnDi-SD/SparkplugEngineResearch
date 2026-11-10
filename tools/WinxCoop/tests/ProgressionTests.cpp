#include "../src/Progression/ProgressionSession.h"
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace winxcoop::progression;

namespace
{
    void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
    Digest Hash(std::uint8_t value){Digest result{};result.fill(value);return result;}
    ConnectionIdentity Link(){return {17,29,43};}
    ChannelGeneration Channels(std::uint64_t base)
    {return {base+100,base+1,base+5,base+1,base+2,base+3,base+4};}
    LevelDescriptor Descriptor(std::uint32_t level=28,std::uint8_t hash=1){return {level,0,Hash(hash)};}
    TrustedInitialRunning Initial(){return {{1,1},Descriptor(),Channels(1000)};}
    Decision Next(std::uint64_t operation=7,std::uint32_t previous=1)
    {return {operation,{previous,previous},{previous+1,previous+1},Reason::LevelChange,Descriptor(29,2)};}
    NativeRetirementReceipt Retired(SceneIdentity scene={1,1})
    {return {scene,true,true,true,true,true,true,true,true,true};}
    NativeEntryReceipt Entry(const Decision& decision,ChannelGeneration channels=Channels(2000),std::uint64_t ordinal=2)
    {return {decision.next,decision.destination,channels,Hash(8),Hash(9),ordinal,true,true};}
    NativeWorldAppliedReceipt Applied(const Decision& decision,ChannelGeneration channels=Channels(2000))
    {return {decision.next,channels,55,73,true,true};}
    Message Prepare(const Decision& decision=Next())
    {Message value;value.kind=Kind::Prepare;value.connection=Link();value.decision=decision;return value;}
    Message NativeRetired(const Decision& decision=Next())
    {auto value=Prepare(decision);value.kind=Kind::Retired;value.retirementMask=CompleteRetirementMask;return value;}
    Message Load(const Decision& decision=Next(),ChannelGeneration channels=Channels(2000))
    {auto value=Prepare(decision);value.kind=Kind::Load;value.channels=channels;value.seed=Hash(8);value.witness=Hash(9);return value;}
    Message Loaded(const Decision& decision=Next(),ChannelGeneration channels=Channels(2000))
    {auto value=Load(decision,channels);value.kind=Kind::Loaded;value.appliedRevision=55;value.nativeTick=73;return value;}
    Message Wire(const Message& value)
    {
        auto encoded=Encode(value);Check(encoded&&encoded->size()==244&&encoded->size()<=256,"one original-capacity application frame");
        auto decoded=Decode({ApplicationClass,static_cast<std::uint32_t>(value.kind),*encoded});
        Check(decoded&&*decoded==value,"common MemoryStream preserves every progression field");return *decoded;
    }
    Message Transfer(Session& from,Session& to,std::uint64_t now)
    {
        auto value=from.PeekOutbound();Check(value.has_value(),"outbound control exists");
        Check(from.PeekOutbound()&&*from.PeekOutbound()==*value,"Peek never consumes or changes a pending frame");
        Check(from.CommitOutbound(now),"full application transport acceptance commits once");
        Check(to.Receive(Wire(*value),now),"opposite progression owner accepts committed control");return *value;
    }
    struct Pair
    {
        Session host{Role::Host,Link(),Initial(),1000},guest{Role::Guest,Link(),Initial(),1000};
        std::vector<Message> messages;
        void Run(const Decision& decision,std::uint64_t now,ChannelGeneration channels)
        {
            Check(host.InputAllowed()&&guest.InputAllowed(),"explicit trusted bootstrap is running");
            Check(host.Begin(decision,now)&&!host.InputAllowed(),"Host immediately fences old input");
            Check(host.ObserveLocalRetired(Retired(decision.previous),now+1),"Host native owner retires all old resources");
            Check(host.ObserveHostEntry(Entry(decision,channels,2),now+2),"Host can load before Guest finishes its cleanup");
            Check(host.PeekOutbound()->kind==Kind::Prepare,"Load cannot overtake Prepare");
            messages.push_back(Transfer(host,guest,now+3));
            Check(!guest.InputAllowed(),"Prepare fences Guest input before its unload");
            Check(guest.ObserveLocalRetired(Retired(decision.previous),now+4),"Guest old native resources are retired");
            Check(!host.PeekOutbound(),"Host entry alone does not publish Load");
            messages.push_back(Transfer(guest,host,now+5));
            Check(host.PeekOutbound()->kind==Kind::Load,"both retirement receipts plus Host entry authorize Load");
            messages.push_back(Transfer(host,guest,now+6));
            auto load=guest.TakeLoadAuthorization(now+7);Check(load&&load->decision==decision,"Guest consumes exact Host load authority");
            Check(!guest.TakeLoadAuthorization(now+7),"one authority never starts a second native load");
            // Host local ordinal2 and Guest local ordinal1 deliberately differ.
            Check(guest.ObserveGuestEntryApplied(Entry(decision,channels,1),Applied(decision,channels),now+8),"Guest new entry and full native ACK qualify");
            Check(!host.InputAllowed()&&!guest.InputAllowed(),"actual new native state still waits for Resume");
            messages.push_back(Transfer(guest,host,now+9));
            Check(host.State()==Phase::ResumePending&&!host.InputAllowed()&&!guest.InputAllowed(),"queued Resume does not release Host or Guest");
            Check(host.Receive(messages.back(),now+9)&&host.OutboundCount()==1,"exact duplicate Loaded does not queue another Resume");
            messages.push_back(Transfer(host,guest,now+10));
            Check(host.InputAllowed()&&guest.InputAllowed(),"only committed/received Resume releases both barriers");
            Check(host.CurrentScene()==decision.next&&guest.CurrentScene()==decision.next,"both owners advance exactly one scene generation");
            Check(host.CurrentChannels()==channels&&guest.CurrentChannels()==channels,"fresh endpoint identity becomes current only at Resume");
        }
    };
    void FullReplay()
    {
        Pair pair;auto first=Next();pair.Run(first,100,Channels(2000));
        Check(pair.host.CompletedOperation()==7&&pair.guest.CompletedOperation()==7,"completed operation retained");
        for(const auto& message:pair.messages)
        {
            auto& receiver=message.kind==Kind::Retired||message.kind==Kind::Loaded?pair.host:pair.guest;
            Check(receiver.Receive(message,120),"completed exact controls replay without native effects");
        }
        Check(pair.host.OutboundCount()==0&&pair.guest.OutboundCount()==0,"replay does not enqueue controls");
        auto second=Next(8,2);second.reason=Reason::Reload;
        pair.Run(second,200,Channels(3000));
        Check(pair.host.CompletedOperation()==8&&pair.guest.CompletedOperation()==8,"successive transition operation is monotonic");
        auto old=pair.messages[3];Check(pair.host.Receive(old,220)&&pair.host.InputAllowed(),"obsolete previous-generation Loaded is ignored");
    }
    void WireValidation()
    {
        for(auto value:{Prepare(),NativeRetired(),Load(),Loaded()})Wire(value);
        auto resume=Loaded();resume.kind=Kind::Resume;Wire(resume);
        auto abort=Prepare();abort.kind=Kind::Abort;abort.terminal=Terminal::Disconnected;Wire(abort);
        auto encoded=*Encode(Prepare());
        auto raw32=[&](std::size_t at){return std::uint32_t(encoded[at])|(std::uint32_t(encoded[at+1])<<8)|
            (std::uint32_t(encoded[at+2])<<16)|(std::uint32_t(encoded[at+3])<<24);};
        Check(raw32(0)==1&&raw32(4)==1&&raw32(40)==1&&raw32(48)==2&&raw32(60)==29&&encoded[68]==2,
            "independent fixed-offset little-endian wire layout");
        Check(!Decode({ApplicationClass+1,1,encoded}),"unrelated application class rejected");
        Check(!Decode({ApplicationClass,2,encoded}),"packet key and kind must agree");
        encoded.pop_back();Check(!Decode({ApplicationClass,1,encoded}),"short record rejected");
        encoded=*Encode(Prepare());encoded[0]=2;Check(!Decode({ApplicationClass,1,encoded}),"unknown protocol version rejected");
        auto bad=Prepare();bad.channels=Channels(2000);Check(!Encode(bad),"Prepare cannot smuggle load or endpoint authority");
        bad=NativeRetired();bad.retirementMask^=1;Check(!Encode(bad),"partial retirement cannot cross the wire as complete");
        bad=Loaded();bad.appliedRevision=0;Check(!Encode(bad),"zero native ACK rejected");
        bad=Prepare();bad.decision.operation=0;Check(!Encode(bad),"zero operation cannot wrap a generation");
        bad=Prepare();bad.decision.next.generation=1;Check(!Encode(bad),"unchanged native generation rejected");
        bad=Load();bad.channels.guestWorldNonce=bad.channels.guestInputNonce;Check(!Encode(bad),"aliased new mappings rejected");
        bad=Load();bad.witness={};Check(!Encode(bad),"missing authoritative witness rejected");
    }
    void GuestCannotChoose()
    {
        Session guest(Role::Guest,Link(),Initial(),1000);
        Check(!guest.Begin(Next(),100)&&guest.Failure()==Terminal::Authority&&!guest.InputAllowed(),"Guest API cannot authorize a transition");
        Session host(Role::Host,Link(),Initial(),1000);
        Check(!host.Receive(Prepare(),100)&&host.Failure()==Terminal::Authority,"peer Prepare is not a Host decision");
    }
    void IncompleteRetirement()
    {
        const std::array<bool NativeRetirementReceipt::*,9> fields={
            &NativeRetirementReceipt::originalExitReturned,&NativeRetirementReceipt::inputRetired,&NativeRetirementReceipt::worldRetired,
            &NativeRetirementReceipt::bindingsRetired,&NativeRetirementReceipt::borrowedBaselinesRestored,&NativeRetirementReceipt::patchesRestored,
            &NativeRetirementReceipt::noNativeOwnership,&NativeRetirementReceipt::noPendingNativeCallsOrApply,&NativeRetirementReceipt::laterWorldResourcesReleased};
        for(auto field:fields)
        {
            Session host(Role::Host,Link(),Initial(),1000);Check(host.Begin(Next(),100),"prepare incomplete-receipt test");
            auto receipt=Retired();receipt.*field=false;
            Check(!host.ObserveLocalRetired(receipt,101)&&host.Failure()==Terminal::NativeReceipt&&!host.InputAllowed(),
                "every native retirement clause is required, not just Exit entry");
        }
    }
    void LoadRequiresBothRetired()
    {
        Session host(Role::Host,Link(),Initial(),1000),guest(Role::Guest,Link(),Initial(),1000);
        Check(host.Begin(Next(),100),"Host begins");Transfer(host,guest,101);
        Check(host.ObserveLocalRetired(Retired(),102)&&host.ObserveHostEntry(Entry(Next()),103),"Host local native state complete");
        Check(!host.PeekOutbound()&&!host.InputAllowed(),"missing Guest retirement prevents Load");
        Check(guest.ObserveLocalRetired(Retired(),104),"Guest retires");Transfer(guest,host,105);
        Check(host.PeekOutbound()&&host.PeekOutbound()->kind==Kind::Load&&!host.InputAllowed(),"Load queued only after both receipts");
    }
    void ReorderingFailsClosed()
    {
        Session host(Role::Host,Link(),Initial(),1000);Check(host.Begin(Next(),100),"Host begins");
        Check(!host.Receive(NativeRetired(),101)&&host.Failure()==Terminal::Sequence,"uncommitted Prepare cannot receive retirement ACK");
        Session guest(Role::Guest,Link(),Initial(),1000);Check(guest.Receive(Prepare(),100),"Guest Prepare");
        Check(!guest.Receive(Load(),101)&&!guest.InputAllowed(),"Load before local retirement fails closed");
        Session waitingHost(Role::Host,Link(),Initial(),1000);Check(waitingHost.Begin(Next(),100),"Host begins next test");
        Check(waitingHost.CommitOutbound(100),"Host Prepare committed");
        Check(!waitingHost.Receive(Loaded(),101),"Loaded cannot precede actual Load commit");
    }
    void ExactDecisionsAndReceipts()
    {
        Session guest(Role::Guest,Link(),Initial(),1000);Check(guest.Receive(Prepare(),100),"Guest Prepare");
        auto changed=Prepare();changed.decision.destination.digest[0]^=1;
        Check(!guest.Receive(changed,101)&&guest.Failure()==Terminal::Identity,"same operation cannot change destination descriptor");
        Session host(Role::Host,Link(),Initial(),1000);Check(host.Begin(Next(),100)&&host.ObserveLocalRetired(Retired(),101),"Host retired");
        auto old=Entry(Next());old.channels=Channels(1000);
        Check(!host.ObserveHostEntry(old,102),"fresh scene cannot inherit old transport channels");
        Session reused(Role::Host,Link(),Initial(),1000);Check(reused.Begin(Next(),100)&&reused.ObserveLocalRetired(Retired(),101),"Host retired again");
        auto nonce=Entry(Next());nonce.channels.guestInputNonce=Channels(1000).hostWorldNonce;
        Check(!reused.ObserveHostEntry(nonce,102),"one reused old nonce fails fresh endpoint admission");
        Session noEntry(Role::Host,Link(),Initial(),1000);Check(noEntry.Begin(Next(),100),"Host begins without retirement");
        Check(!noEntry.ObserveHostEntry(Entry(Next()),101),"new entry cannot replace old retirement");
    }
    Session ReadyGuest()
    {
        Session guest(Role::Guest,Link(),Initial(),1000);
        Check(guest.Receive(Prepare(),100)&&guest.ObserveLocalRetired(Retired(),101)&&guest.CommitOutbound(102)&&guest.Receive(Load(),103),"Guest loading fixture");
        Check(guest.TakeLoadAuthorization(104).has_value(),"Guest takes authority");return guest;
    }
    void NativeApplyNeedsExactEntry()
    {
        auto guest=ReadyGuest();auto wrong=Entry(Next(),Channels(2000),1);wrong.seed[0]^=1;
        Check(!guest.ObserveGuestEntryApplied(wrong,Applied(Next()),105)&&guest.Failure()==Terminal::NativeReceipt,"Guest seed must match Host payload digest");
        auto zero=ReadyGuest();auto applied=Applied(Next());applied.revision=0;
        Check(!zero.ObserveGuestEntryApplied(Entry(Next()),applied,105),"no zero/full-apply substitute");
        auto notNative=ReadyGuest();applied=Applied(Next());applied.completeNativeSnapshotApplied=false;
        Check(!notNative.ObserveGuestEntryApplied(Entry(Next()),applied,105),"network receipt cannot qualify actual native apply");
        auto stale=ReadyGuest();applied=Applied(Next());applied.scene={1,1};
        Check(!stale.ObserveGuestEntryApplied(Entry(Next()),applied,105),"previous-generation ACK rejected");
        auto noNeutral=ReadyGuest();applied=Applied(Next());applied.freshNeutralInputBoundaryObserved=false;
        Check(!noNeutral.ObserveGuestEntryApplied(Entry(Next()),applied,105),"new input needs a fresh neutral native boundary");
    }
    void DuplicatesAndBackpressure()
    {
        Session host(Role::Host,Link(),Initial(),1000);Check(host.Begin(Next(),100),"Host begins");
        auto expected=*host.PeekOutbound();
        for(unsigned i=0;i<1000;++i)Check(host.Begin(Next(),101)&&host.OutboundCount()==1&&*host.PeekOutbound()==expected,"duplicates preserve one exact blocked frame");
        Check(host.Deadline()==1100&&!host.InputAllowed(),"blocked transport never commits or refreshes deadline");
        Check(!host.Poll(1100)&&host.Failure()==Terminal::Timeout&&!host.PeekOutbound(),"uncommitted blocked frame times out terminally");
        Session guest(Role::Guest,Link(),Initial(),1000);Check(guest.Receive(Prepare(),100)&&guest.Receive(Prepare(),1099),"exact duplicate remains bounded");
        Check(!guest.Receive(Prepare(),1100)&&guest.Failure()==Terminal::Timeout,"deadline checked before duplicate Receive");
    }
    void ClockAndDisconnect()
    {
        Session host(Role::Host,Link(),Initial(),1000);Check(host.Begin(Next(),100),"Host begins");
        Check(!host.Receive(NativeRetired(),99)&&host.Failure()==Terminal::Clock,"every Receive checks monotonic time first");
        Session retired(Role::Host,Link(),Initial(),1000);Check(retired.Begin(Next(),100),"Host begins retirement test");
        Check(!retired.ObserveLocalRetired(Retired(),1100)&&retired.Failure()==Terminal::Timeout,"trusted receipt cannot bypass deadline");
        Session stopped(Role::Host,Link(),Initial(),1000);stopped.Stop();
        Check(!stopped.Begin(Next(),100)&&!stopped.InputAllowed()&&!stopped.PeekOutbound(),"disconnect is terminal without pending native authority");
        Session overflow(Role::Host,Link(),Initial(),1000);
        Check(!overflow.Begin(Next(),(std::numeric_limits<std::uint64_t>::max)()-500)&&overflow.Failure()==Terminal::CounterExhausted,
            "deadline overflow cannot create an immortal operation");
    }
    void ResumeCommitIsAuthority()
    {
        auto guest=ReadyGuest();Check(guest.ObserveGuestEntryApplied(Entry(Next(),Channels(2000),1),Applied(Next()),105),"Guest native apply ready");
        auto resume=Loaded();resume.kind=Kind::Resume;
        Check(!guest.Receive(resume,106)&&!guest.InputAllowed(),"Resume cannot overtake Guest native-ACK transport commit");
        Session unknown(Role::Guest,Link(),Initial(),1000);auto m=Prepare();m.connection.token++;
        Check(!unknown.Receive(m,100)&&unknown.Failure()==Terminal::Identity,"unrelated control connection cannot authorize native work");
    }
}
int main()
{
    const std::pair<const char*,void(*)()> tests[]={
        {"full_replay_and_two_generations",FullReplay},{"wire_validation",WireValidation},{"host_authority",GuestCannotChoose},
        {"all_native_retirement_clauses",IncompleteRetirement},{"both_retired_before_load",LoadRequiresBothRetired},
        {"reordered_controls",ReorderingFailsClosed},{"exact_decisions_and_fresh_channels",ExactDecisionsAndReceipts},
        {"actual_entry_and_native_ack",NativeApplyNeedsExactEntry},{"duplicates_and_transport_backpressure",DuplicatesAndBackpressure},
        {"deadlines_clock_disconnect",ClockAndDisconnect},{"resume_commit_authority",ResumeCommitIsAuthority}};
    unsigned passed=0;
    try
    {
        for(const auto& test:tests){test.second();++passed;std::cout<<"PASS "<<test.first<<'\n';}
    }
    catch(const std::exception& error){std::cerr<<"FAIL after "<<passed<<": "<<error.what()<<'\n';return 1;}
    std::cout<<"Progression contract checks: "<<passed<<'/'<<std::size(tests)<<"; native callbacks are trusted fixtures, gameStarted=false\n";
    return 0;
}
