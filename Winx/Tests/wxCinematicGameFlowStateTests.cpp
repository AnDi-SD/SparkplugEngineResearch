#include "Code/wxCinematicGameFlowState.h"
#include "Analysis/PC/wxCinematicGameFlowStateAbi.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
    void* Token(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::string Join(const std::vector<std::string>& values)
    {
        std::string result;
        for(const auto& value:values) { if(!result.empty())result+=',';result+=value; }
        return result.empty()?"-":result;
    }
    struct Host final : wxCinematicGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        wxCinematicFlagsForAnalysis flags;
        wxCinematicGameFlowState* active=nullptr;
        unsigned currentOwner=0,mutations=0;
        std::array<unsigned,2> inputIDs{1,3};
        std::vector<std::string> effects;
        std::optional<std::uint32_t> nativeReceiverWord=0x21123480;
        explicit Host(std::uint32_t seed=0)
        { for(std::size_t i=0;i<flags.size();++i)flags[i]=(static_cast<std::uint32_t>(i)*0x10203)^seed; }
        void* BorrowInputForAnalysis() override { return Token(0x18); }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {1200,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        wxCinematicFlagsForAnalysis& CinematicFlagsForAnalysis() override { return flags; }
        std::optional<std::uint32_t> CinematicNativeReceiverWordForAnalysis(
            const wxCinematicGameFlowState&) const noexcept override { return nativeReceiverWord; }
        void* ResolveCinematicInputOwnerForAnalysis() override { return Token(100+currentOwner); }
        void* ReadCinematicInput1CForAnalysis(void* owner) override
        {
            const auto index=reinterpret_cast<std::uintptr_t>(owner)-100;
            Check(index<2,"Captured input owner");return Token(inputIDs[index]);
        }
        void ResetCinematicInputForAnalysis(void* input) override
        {
            Check(active!=nullptr,"Input callback sees actual source Cinematic");
            auto& s=active->CinematicStateForFixture();
            effects.push_back("R"+std::to_string(reinterpret_cast<std::uintptr_t>(input))+":"
                +std::to_string(s.field84)+":"+std::to_string(s.fieldE0));
            if(mutations&1)inputIDs[0]=2;
            if(mutations&2)currentOwner=1;
            if(mutations&4) { s.field7C=0x12345678;s.field84=0xEE;s.fieldE0=0xDD; }
        }
        void SetCinematicInputModeForAnalysis(void* input,std::uint32_t mode) override
        {
            auto& s=active->CinematicStateForFixture();
            effects.push_back("M"+std::to_string(reinterpret_cast<std::uintptr_t>(input))+":"
                +std::to_string(mode)+":"+std::to_string(s.field84)+":"+std::to_string(s.fieldE0));
            if(mutations&8) { s.field84=0xAA;s.fieldE0=0xCC; }
        }
    };
    struct Receiver final : spBaseObject
    {
        Host* host=nullptr;
        unsigned id=0;
        void vfunc_0C(const void* value) noexcept override
        {
            const auto& packet=*static_cast<const spNotificationForAnalysis*>(value);
            auto& s=host->active->CinematicStateForFixture();
            host->effects.push_back("N"+std::to_string(id)+":"+std::to_string(packet.code)+":"
                +std::to_string(packet.word04)+":"+std::to_string(packet.word08)+":"
                +std::to_string(packet.word0C)+":"+std::to_string(packet.sender==host->active)+":"
                +std::to_string(packet.word14)+":"+std::to_string(packet.payload)+":"
                +std::to_string(packet.tail)+":"+std::to_string(s.field84)+":"+std::to_string(s.fieldE0));
            if(id==1 && (host->mutations&16))
            { host->currentOwner=1;host->inputIDs[0]=2;s.field7C=0x87654321;s.field84=0xBB;s.fieldE0=0xAB; }
            if(id==1 && (host->mutations&32))
            {
                auto& mutablePacket=*const_cast<spNotificationForAnalysis*>(&packet);
                mutablePacket.code=0x12345678;mutablePacket.payload=0xFFFFFFFF;mutablePacket.tail=0x80000000;
            }
        }
    };
    void Print(const wxCinematicGameFlowState& object,const Host& host)
    {
        const auto& s=object.GetCinematicStateForAnalysis();
        std::cout<<s.field7C<<' '<<s.field80<<' '<<unsigned(s.field84)<<' '<<s.field88<<' '
            <<unsigned(s.fieldDE)<<' '<<unsigned(s.fieldE0);
        for(const auto value:s.flags8C)std::cout<<' '<<unsigned(value);
        for(const auto value:host.flags)std::cout<<' '<<value;
        std::cout<<' '<<host.currentOwner+1<<' '<<host.inputIDs[0]<<' '<<Join(host.effects)<<'\n';
    }
    void Replay()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            std::istringstream input(line);std::string op;
            std::uint32_t code7C,word80,word88,seed,mutations,code,payload,nativeReceiverWord;
            unsigned flag84,flagDE,flagE0;
            if(!(input>>op>>code7C>>word80>>flag84>>word88>>flagDE>>flagE0>>seed>>mutations>>code>>payload>>nativeReceiverWord)
                ||flag84>255||flagDE>255||flagE0>255)throw std::invalid_argument("Cinematic replay input");
            spSubscriptionManager manager;Host host(seed);
            std::array<Receiver,2> receivers;
            for(unsigned i=0;i<receivers.size();++i)
            { receivers[i].host=&host;receivers[i].id=i+1;Check(manager.SubscribeForAnalysis(0x13,receivers[i]),"Shared output subscription"); }
            wxCinematicGameFlowState object(host);host.active=&object;host.mutations=mutations;
            host.nativeReceiverWord=nativeReceiverWord;
            auto& s=object.CinematicStateForFixture();
            if(op!="ctor")
            {
                s.field7C=code7C;s.field80=word80;s.field84=static_cast<std::uint8_t>(flag84);s.field88=word88;
                s.fieldDE=static_cast<std::uint8_t>(flagDE);s.fieldE0=static_cast<std::uint8_t>(flagE0);
                for(std::size_t i=0;i<s.flags8C.size();++i)
                { s.flags8C[i]=static_cast<std::uint8_t>(i*17+seed);host.flags[i]=(static_cast<std::uint32_t>(i)*0x10203)^seed; }
            }
            if(op=="enter")object.FlowSlot20ForAnalysis();
            else if(op=="exit")object.FlowSlot24ForAnalysis();
            else if(op=="reset")object.ResetCinematicSelectionsForAnalysis();
            else if(op=="notify")
            { spNotificationForAnalysis message;message.code=code;message.payload=payload;object.NotifyCinematicForAnalysis(message); }
            else if(op!="ctor")throw std::invalid_argument("Unknown Cinematic operation");
            Print(object,host);
        }
    }
    void Contracts()
    {
        Check(!spSubscriptionManager::GetInstance(),"Fresh shared manager state");
        Host missing;
        bool refused=false;
        try { wxCinematicGameFlowState object(missing); }catch(const std::logic_error&) { refused=true; }
        Check(refused,"Absent subscriptions are an explicit analytical refusal");
        spSubscriptionManager manager;Host host(0xFFFFFFFF);
        {
            wxCinematicGameFlowState source(host);host.active=&source;
            Check(manager.GetSubscriberCountForAnalysis(0x10)==1,"Constructor subscribes real source object");
            for(std::size_t i=0;i<host.flags.size();++i)
                Check(host.flags[i]==(((static_cast<std::uint32_t>(i)*0x10203)^0xFFFFFFFF)&0xFFFFFFFE),"Constructor clears only global bit0");
            auto& s=source.CinematicStateForFixture();s.field84=255;s.fieldE0=99;
            host.mutations=15;source.FlowSlot20ForAnalysis();
            Check(Join(host.effects)=="R1:255:99,M2:6:238:221"&&s.field84==1&&s.fieldE0==0,
                "Entry captures owner, reloads its member and overwrites flags after callbacks");
            spNotificationForAnalysis message;message.code=0x27A3;message.payload=0x80000100;
            source.NotifyCinematicForAnalysis(message);Check(s.field84==0,"Notify tests only low payload byte");
            s.field84=77;message.payload=0x100000001ULL;source.NotifyCinematicForAnalysis(message);
            Check(s.field84==77,"Nonzero low byte preserves state");
            s.field7C=0xAABBCCDD;s.fieldDE=0xFF;s.field88=123;
            s.flags8C.fill(42);host.flags.fill(0xA5A5A5A5);source.ResetCinematicSelectionsForAnalysis();
            Check(s.field84==0&&s.field7C==0xAABBCCDD&&s.fieldDE==255&&s.field88==123,"Reset preserves unrelated own fields");
            for(std::size_t i=0;i<host.flags.size();++i)Check(host.flags[i]==0xA5A5A5A4&&s.flags8C[i]==0,"Reset82 pairs");
            wxCinematicGameFlowState target(host);target.CinematicStateForFixture().field84=33;
            spCloneManager clones;Check(source.vfunc_14(target,clones)&&target.GetCinematicStateForAnalysis().field84==33,"Copy remains actual base no-op");
            auto clone=clones.Clone(source);auto* typed=dynamic_cast<wxCinematicGameFlowState*>(clone.get());
            Check(typed&&typed->GetCinematicStateForAnalysis().field7C==0&&typed->GetCinematicStateForAnalysis().field84==0,
                "Clone constructs fresh own defaults");
            Check(manager.GetSubscriberCountForAnalysis(0x10)==3,"Source Clone uses actual shared subscription");
            refused=false;try { source.FlowSlot30ForAnalysis(); }catch(const std::logic_error&) { refused=true; }
            Check(refused,"Movie/load slot remains unqualified");
            host.effects.clear();host.nativeReceiverWord.reset();s.field84=55;s.fieldE0=77;
            refused=false;try { source.FlowSlot24ForAnalysis(); }catch(const std::logic_error&) { refused=true; }
            Check(refused&&host.effects.empty()&&s.field84==55&&s.fieldE0==77,
                "Missing native receiver binding refuses before dispatch/input effects or stores");
        }
        Check(manager.GetSubscriberCountForAnalysis(0x10)==0,"All source Cinematic subscriptions released");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else if(argc==1) { Contracts();std::cout<<"Cinematic contracts passed\n"; }
        else return 1;
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
