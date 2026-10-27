#include "Code/wxHurtAIAction.h"
#include "Code/wxGameTimer.h"
#include "Code/wxPerception.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxHurtAIActionAbi.h"
#include "Analysis/PS2/wxHurtAIActionAbi.h"
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Result=wxHurtResultForAnalysis;
    using Profile=wxHurtUpdateProfileForAnalysis;
    using Platform=wxHurtPlatformForAnalysis;
    void Check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
    struct World final:wxHurtAIActionHost,wxPerceptionHost
    {
        int owner=1;
        std::array<std::uint8_t,0x74> command{};
        spNode ownerNode,playerNode;
        spTimer clock;
        wxGameTimer timer{clock};
        wxPerception perception;
        std::uint32_t state=0;
        bool commandPresent=true;
        unsigned resets=0,selections=0;
        std::string trace;
        World() { command.fill(0xA5); }
        ~World() override { perception.ResetForAnalysis(); }
        void* ReadCommandForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="command;";return commandPresent?command.data():nullptr; }
        void ResetCommandForAnalysis(void* value) noexcept override
        {
            if (value!=command.data()) std::terminate();trace+="reset;";++resets;
            // Literal qualified foreign command payload, not an owned Hurt
            // handler. Preserve every byte not written by5992E0/3730E0.
            for (const auto offset:{4U,8U,12U,16U,20U,0x6CU})
                for (unsigned i=0;i<4;++i) command[offset+i]=0;
            for (unsigned i=0x19;i<=0x31;++i) command[i]=0;
            for (unsigned i=0x5C;i<=0x60;++i) command[i]=0;
            command[0x64]=command[0x70]=0;
        }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {
            if (value!=command.data() || offset>=command.size()) std::terminate();
            command[offset]=byte;trace+="byte:"+std::to_string(offset)+";";
        }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        {
            if (value!=command.data() || offset+4>command.size()) std::terminate();
            std::memcpy(command.data()+offset,&word,4);trace+="word:"+std::to_string(offset)+";";
        }
        std::uint32_t ReadCharacterStateForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="state;";return state; }
        spNode& ReadOwnerNodeForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="owner-node;";return ownerNode; }
        spNode& ReadPlayerNodeForAnalysis() noexcept override { trace+="player-node;";return playerNode; }
        wxPerception& ReadPerceptionForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="perception;";return perception; }
        wxGameTimer& GameTimerForAnalysis() noexcept override { trace+="timer;";return timer; }
        void SelectActionForAnalysis(void* actor,std::uint32_t key,std::uint32_t parameter) noexcept override
        { if (actor!=&owner || key!=2 || parameter!=0) std::terminate();trace+="selector2;";++selections; }
        std::array<float,3> ReadNodePositionForAnalysis(const void*) const override { return {}; }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
        void Setup(bool hasOwner,std::uint32_t paused,std::uint32_t now,std::uint32_t limit,
            std::uint32_t opaque,std::uint32_t expiry,const spNode::Vector3& origin,const spNode::Vector3& player)
        {
            perception.BindForAnalysis(*this,hasOwner?&owner:nullptr,{});
            auto& p=perception.StateForAnalysis();p.word3C=limit;p.byte40=0xA5;p.opaque68=opaque;p.opaque6C=expiry;
            timer.SetPausedByteForAnalysis(std::uint8_t(paused));
            auto time=timer.GetStateForAnalysis();time.currentMilliseconds=now;timer.SetStateForAnalysis(time);
            ownerNode.SetPositionForAnalysis(origin);playerNode.SetPositionForAnalysis(player);
        }
        std::string Payload() const
        {
            constexpr char hex[]="0123456789abcdef";std::string result;
            for (unsigned i=4;i<command.size();++i) { result+=hex[command[i]>>4];result+=hex[command[i]&15]; }
            return result;
        }
    };
    void Replay()
    {
        std::string operation;
        while (std::cin>>operation)
        {
            if (operation=="c")
            {
                unsigned from,to;std::cin>>from>>to;World world;
                wxHurtAIAction source(world),destination(world);source.SetOwnerForAnalysis(&world.owner);
                source.SetLatchForAnalysis(std::uint8_t(from));destination.SetLatchForAnalysis(std::uint8_t(to));
                spCloneManager manager;const bool copied=source.vfunc_14(destination,manager);
                auto clone=manager.Clone(source);auto* hurt=dynamic_cast<wxHurtAIAction*>(clone.get());
                Check(hurt!=nullptr,"typed Hurt replay clone");
                std::cout<<unsigned(copied)<<' '<<unsigned(destination.LatchForAnalysis())<<' '
                    <<unsigned(hurt->LatchForAnalysis())<<' '<<unsigned(hurt->GetOwnerForAnalysis()!=nullptr)<<'\n';
                continue;
            }
            const bool ps2=operation.size()==2 && operation.front()=='p';
            if (ps2) operation=operation.substr(1);
            unsigned flag,state,paused,hasOwner,opaque,expiry,now,limit,argument,present;
            spNode::Vector3 origin{},player{};
            std::cin>>flag>>state>>paused>>hasOwner>>opaque>>expiry>>now>>limit;
            for (auto& value:origin) std::cin>>value;for (auto& value:player) std::cin>>value;
            std::cin>>argument>>present;Check(bool(std::cin),"complete Hurt replay input");
            World world;world.state=state;world.commandPresent=present!=0;
            world.Setup(hasOwner!=0,paused,now,limit,opaque,expiry,origin,player);
            wxHurtAIAction action(world,ps2?Platform::PS2:Platform::PC,Profile::PcSmallIntegerRange);
            action.SetOwnerForAnalysis(&world.owner);action.SetLatchForAnalysis(std::uint8_t(flag));
            unsigned result=0;
            if (operation=="e") action.EnterForAnalysis(argument);
            else if (operation=="x") action.vfunc_34_ClearForAnalysis();
            else if (operation=="u") result=unsigned(action.UpdateForAnalysis());
            else if (operation=="r") result=unsigned(world.perception.RefreshBoundedPcForAnalysis(world.timer));
            else if (operation=="n") { wxAIActionMessageForAnalysis message{0x1C};action.vfunc_0C(&message); }
            else if (operation=="q") action.vfunc_38();
            else throw std::invalid_argument("Hurt replay operation");
            const auto& p=world.perception.StateForAnalysis();
            std::cout<<result<<' '<<unsigned(action.LatchForAnalysis())<<' '<<p.word3C<<' '
                <<unsigned(p.byte40)<<' '<<p.opaque68<<' '<<p.opaque6C<<' '<<world.Payload()<<'\n';
        }
    }
    void HooksAndCopy()
    {
        World world;wxHurtAIAction source(world),destination(world);source.SetOwnerForAnalysis(&world.owner);
        Check(source.LatchForAnalysis()==0,"constructor only own latch zero");
        source.SetLatchForAnalysis(7);source.EnterForAnalysis(123);
        Check(source.LatchForAnalysis()==0 && world.command[0x5C]==1 && world.command[0x60]==1 &&
            world.trace=="command;byte:92;command;byte:96;command;word:4;","entry rereads command before each store, latch last");
        source.EnterForAnalysis(0);Check(world.command[0x60]==0,"entry normalizes zero argument");
        world.commandPresent=false;source.SetLatchForAnalysis(7);source.EnterForAnalysis(1);
        Check(source.LatchForAnalysis()==7,"null Entry command preserves latch");
        world.commandPresent=true;source.vfunc_34_ClearForAnalysis();Check(world.command[0x5C]==0 && source.LatchForAnalysis()==7,"exit only clears byte5C");
        struct Child final:wxAIAction { unsigned messages=0;void vfunc_0C(const void*) noexcept override { ++messages; } } child;
        source.SetCurrentActionForAnalysis(&child);wxAIActionMessageForAnalysis message{0x1C};source.vfunc_0C(&message);
        Check(child.messages==0,"Hurt Notify is a no-op without common forwarding");source.SetCurrentActionForAnalysis(nullptr);
        destination.SetLatchForAnalysis(3);spCloneManager clones;
        Check(source.vfunc_14(destination,clones) && destination.LatchForAnalysis()==3,"Copy does not copy own latch");
        auto copied=clones.Clone(source);auto* hurt=dynamic_cast<wxHurtAIAction*>(copied.get());
        Check(hurt && hurt->LatchForAnalysis()==0 && hurt->GetOwnerForAnalysis()==nullptr,"Clone keeps constructor latch and base runtime owner default");
        wxHurtAIAction::SetFactoryHostForAnalysis(&world);auto factory=spRTTIManager::Instance().Create(wxHurtAIAction::ClassID);
        Check(factory && factory->IsKindOf(wxAIAction::ClassID),"factory RTTI identity");wxHurtAIAction::SetFactoryHostForAnalysis(nullptr);
    }
    void LatchAndRange()
    {
        World world;world.Setup(true,1,100,std::bit_cast<unsigned>(1.0f),0x12345678,200,{0,0,0},{3,4,0});
        wxHurtAIAction action(world,Platform::PC,Profile::PcSmallIntegerRange);action.SetOwnerForAnalysis(&world.owner);
        world.state=10;Check(action.vfunc_24() && action.LatchForAnalysis()==1 && world.resets==1,
            "enter stateA performs qualified command Reset then latches");
        Check(action.vfunc_24() && world.resets==1,"latched stateA waits without repeated Reset");
        world.state=0;world.trace.clear();Check(action.vfunc_24() && action.LastResultForAnalysis()==Result::SelectorRequested &&
            action.LatchForAnalysis()==0 && world.selections==1,"far paused real Perception false still continues to selector2");
        Check(world.trace=="state;owner-node;player-node;perception;perception;perception;timer;selector2;" &&
            world.perception.StateForAnalysis().word3C==std::bit_cast<unsigned>(11025.0f) &&
            world.perception.StateForAnalysis().byte40==0xA5 && world.perception.StateForAnalysis().opaque68==0x12345678,
            "range writes squared extended value before real paused refresh, preserving paused state");
        action.SetLatchForAnalysis(1);world.trace.clear();
        Check(action.vfunc_24() && world.trace=="state;owner-node;player-node;perception;selector2;",
            "comparison uses raw distance5 against stored square11025 and skips refresh");
        world.perception.StateForAnalysis().word3C=std::bit_cast<unsigned>(1.0f);world.timer.SetPausedByteForAnalysis(0);
        action.SetLatchForAnalysis(1);world.selections=0;
        Check(!action.vfunc_24() && action.LastResultForAnalysis()==Result::UnqualifiedPerceptionRefresh &&
            action.LatchForAnalysis()==1 && world.selections==0 && world.perception.StateForAnalysis().byte40==0,
            "owner scan refusal preserves native prefix and stops before latch clear or selector");
        world.playerNode.SetPositionForAnalysis({.5f,0,0});action.SetLatchForAnalysis(1);
        Check(!action.vfunc_24() && action.LastResultForAnalysis()==Result::UnqualifiedNumericDomain,"fractional range domain refuses");
        wxHurtAIAction guarded(world);guarded.SetOwnerForAnalysis(&world.owner);guarded.SetLatchForAnalysis(1);
        Check(!guarded.vfunc_24() && guarded.LastResultForAnalysis()==Result::UnqualifiedRangeHelper,"default profile does not enable range helper");
        wxHurtAIAction ps2(world,Platform::PS2,Profile::PcSmallIntegerRange);ps2.SetOwnerForAnalysis(&world.owner);ps2.SetLatchForAnalysis(1);
        Check(!ps2.vfunc_24() && ps2.LastResultForAnalysis()==Result::UnqualifiedRangeHelper,"PC range profile cannot enable PS2 arithmetic");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if (argc==2 && std::string(argv[1])=="--replay") Replay();
        else { HooksAndCopy();LatchAndRange();std::cout<<"wxHurtAIAction checks passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
