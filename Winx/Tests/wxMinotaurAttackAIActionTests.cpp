#include "Code/wxMinotaurAttackAIAction.h"
#include "Code/wxPerception.h"
#include "Code/wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxMinotaurAttackAIActionAbi.h"
#include "Analysis/PS2/wxMinotaurAttackAIActionAbi.h"
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Result=wxMinotaurAttackResultForAnalysis;
    using Profile=wxMinotaurAttackProfileForAnalysis;
    void Check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
    struct World final:wxMinotaurAttackAIActionHost,wxPerceptionHost
    {
        int owner=1,target=2;
        std::array<std::uint8_t,0x74> command{},otherCommand{};
        spNode ownerNode,targetNode;
        spTimer clock;wxGameTimer timer{clock};wxPerception perception;
        spFunctionEval::RandomStateForAnalysis random;
        std::uint32_t state130=0,selector=0;
        std::string effects;
        wxMinotaurAttackAIAction* action=nullptr;
        bool swapOnState=false,swapOnExitWrite=false,swapOnNotifyWrite=false,swapOnMotionWrite=false;
        World()
        {
            command.fill(0xA5);otherCommand.fill(0x5A);random.state.fill(0);random.index=0;
            ownerNode.SetPositionForAnalysis({111,222,333});ownerNode.MarkLocalTransformDirtyForAnalysis();
            targetNode.SetPositionForAnalysis({0,150,100});targetNode.MarkLocalTransformDirtyForAnalysis();
            Check(ownerNode.UpdateWorldForAnalysis() && targetNode.UpdateWorldForAnalysis(),"actual common Node cached positions");
            // Native Perception consumes LOCAL positions. Keep deliberately
            // different cached WORLD values to expose a wrong binding.
            ownerNode.SetPositionForAnalysis({0,0,0});targetNode.SetPositionForAnalysis({-111,-222,-333});
            Nearest(true);
        }
        ~World() override { perception.ResetForAnalysis(); }
        void Nearest(bool present)
        {
            perception.ResetForAnalysis();perception.BindForAnalysis(*this,&owner,present?std::vector<void*>{&target}:std::vector<void*>{});
            perception.StateForAnalysis().scratchWords30=std::array<std::uint32_t,3>{0xA5A5A5A5,0xA5A5A5A5,0xA5A5A5A5};
        }
        wxPerception& ReadPerceptionForAnalysis(void* value) noexcept override
        { if (value!=&owner) std::terminate();return perception; }
        wxGameTimer& GameTimerForAnalysis() noexcept override { return timer; }
        spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept override { return random; }
        void* ReadCommandForAnalysis(void* value) noexcept override
        { if (value!=&owner) std::terminate();return command.data(); }
        std::uint8_t ReadCommandByteForAnalysis(void* value,std::uint32_t offset) noexcept override
        { if ((value!=command.data() && value!=otherCommand.data()) || offset>=command.size()) std::terminate();return static_cast<std::uint8_t*>(value)[offset]; }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {
            if ((value!=command.data() && value!=otherCommand.data()) || offset>=command.size()) std::terminate();
            static_cast<std::uint8_t*>(value)[offset]=byte;
            effects+=std::to_string(offset+(value==otherCommand.data()?0x400:0))+",1,"+std::to_string(byte)+";";
            if (swapOnExitWrite && offset==0x1D) { action->StateForAnalysis().command=otherCommand.data();swapOnExitWrite=false; }
            if (swapOnNotifyWrite && offset==0x60) { action->StateForAnalysis().command=otherCommand.data();swapOnNotifyWrite=false; }
        }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        {
            if ((value!=command.data() && value!=otherCommand.data()) || offset+4>command.size()) std::terminate();
            std::memcpy(static_cast<std::uint8_t*>(value)+offset,&word,4);
            effects+=std::to_string(offset+(value==otherCommand.data()?0x400:0))+",4,"+std::to_string(word)+";";
            if (swapOnMotionWrite && offset==4) { action->StateForAnalysis().command=otherCommand.data();swapOnMotionWrite=false; }
        }
        spNode& ReadNodeForAnalysis(void* value) noexcept override
        { if (value==&owner) return ownerNode;if (value==&target) return targetNode;std::terminate(); }
        void Geometry(const std::array<std::uint32_t,3>& origin,const std::array<std::uint32_t,3>& targetWords)
        {
            spNode::Vector3 p{},q{};for (unsigned i=0;i!=3;++i) { p[i]=std::bit_cast<float>(origin[i]);q[i]=std::bit_cast<float>(targetWords[i]); }
            ownerNode.SetPositionForAnalysis(p);ownerNode.MarkLocalTransformDirtyForAnalysis();
            targetNode.SetPositionForAnalysis(q);targetNode.MarkLocalTransformDirtyForAnalysis();
            Check(ownerNode.UpdateWorldForAnalysis() && targetNode.UpdateWorldForAnalysis(),"real phase2 cached WORLD positions");
            ownerNode.SetPositionForAnalysis({901,902,903});targetNode.SetPositionForAnalysis({-901,-902,-903});
        }
        std::uint32_t ReadCharacterState130ForAnalysis(void* value) noexcept override
        {
            if (value!=&owner) std::terminate();
            if (swapOnState) { action->StateForAnalysis().command=otherCommand.data();swapOnState=false; }
            return state130;
        }
        void SelectActionForAnalysis(void* value,std::uint32_t key,std::uint32_t parameter) noexcept override
        { if (value!=&owner || key!=1 || parameter!=0) std::terminate();selector=key; }
        std::array<float,3> ReadNodePositionForAnalysis(const void* value) const override
        { if (value==&owner) return ownerNode.GetPositionForAnalysis();if (value==&target) return targetNode.GetPositionForAnalysis();std::terminate(); }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
        std::string Payload() const
        {
            constexpr char hex[]="0123456789abcdef";std::string result;
            for (unsigned i=4;i<command.size();++i) { result+=hex[command[i]>>4];result+=hex[command[i]&15]; }
            return result;
        }
    };
    void Fields(std::ostream& out,const wxMinotaurAttackStateForAnalysis& s,const void* target,const void* command,const void* otherCommand=nullptr)
    {
        out<<unsigned(s.target==target)<<' '<<s.phase<<' '<<s.deadline<<' '<<s.randomWord3B4<<' '<<s.word3B8<<' '
            <<(s.command==command?1:otherCommand && s.command==otherCommand?2:0)<<' '<<s.word3C4.value_or(0)<<' '<<unsigned(s.byte3BC)<<' '<<unsigned(s.byte3BD)<<' '<<unsigned(s.word3C4.has_value());
    }
    std::uint32_t Pattern(unsigned offset) { return 0x12345678U+offset*0x10203U; }
    void SeedLifecycle(wxMinotaurAttackStateForAnalysis& s,std::uint32_t target,std::uint32_t command,bool complement)
    {
        const auto value=[&](unsigned offset) { const auto word=offset==0x3A8?target:offset==0x3C0?command:Pattern(offset);return complement?word^0xFFFFFFFFU:word; };
        s.target=reinterpret_cast<void*>(std::uintptr_t(value(0x3A8)));s.phase=value(0x3AC);s.deadline=value(0x3B0);
        s.randomWord3B4=value(0x3B4);s.word3B8=value(0x3B8);s.command=reinterpret_cast<void*>(std::uintptr_t(value(0x3C0)));s.word3C4=value(0x3C4);
        s.byte3BC=s.byte3BD=complement?0x5A:0xA5;
    }
    void Replay()
    {
        std::string operation;
        while (std::cin>>operation)
        {
            World world;wxMinotaurAttackAIAction action(world,operation=="p"?Profile::PcCooldownAndFiniteMovement:Profile::PcCooldown);world.action=&action;
            action.SetOwnerForAnalysis(&world.owner);auto& s=action.StateForAnalysis();
            if (operation=="p")
            {
                std::array<std::uint32_t,3> origin{},target{};unsigned swap;
                for (auto& value:origin) std::cin>>value;for (auto& value:target) std::cin>>value;std::cin>>swap;Check(bool(std::cin),"phase2 geometry replay input");
                Check(action.EnterForAnalysis()==Result::Completed,"actual Entry and Perception produce the target binding");
                s.phase=2;s.deadline=0x12345678;s.randomWord3B4=0x48742400;s.byte3BC=0xA5;
                world.command.fill(0xA5);world.otherCommand.fill(0x5A);world.effects.clear();world.swapOnMotionWrite=swap!=0;world.Geometry(origin,target);
                const auto result=action.UpdateForAnalysis();Check(result==Result::Completed || result==Result::UnqualifiedControllerReset,"bounded whole movement or unavailable reset prefix");
                std::cout<<unsigned(result==Result::Completed)<<' '<<unsigned(result==Result::UnqualifiedControllerReset)<<" 0 ";
                Fields(std::cout,s,&world.target,world.command.data(),world.otherCommand.data());std::cout<<' '<<world.random.index;
                const auto& scratch=world.perception.StateForAnalysis().scratchWords30;Check(scratch.has_value(),"phase2 known scratch");
                for (const auto value:*scratch) std::cout<<' '<<value;std::cout<<' '<<world.Payload()<<' ';
                constexpr char hex[]="0123456789abcdef";for (unsigned i=4;i<world.otherCommand.size();++i) std::cout<<hex[world.otherCommand[i]>>4]<<hex[world.otherCommand[i]&15];
                std::cout<<'|'<<world.effects<<'\n';continue;
            }
            if (operation=="l")
            {
                std::uint32_t target,command;std::cin>>target>>command;wxMinotaurAttackAIAction destination(world);
                const auto* targetPointer=reinterpret_cast<void*>(std::uintptr_t(target));const auto* commandPointer=reinterpret_cast<void*>(std::uintptr_t(command));
                Fields(std::cout,s,targetPointer,commandPointer);std::cout<<' ';
                SeedLifecycle(s,target,command,false);SeedLifecycle(destination.StateForAnalysis(),target,command,true);
                spCloneManager clones;Check(action.vfunc_14(destination,clones),"same-kind literal Copy");
                auto object=clones.Clone(action);auto* clone=dynamic_cast<wxMinotaurAttackAIAction*>(object.get());Check(clone!=nullptr,"actual shared clone map");
                Fields(std::cout,destination.StateForAnalysis(),targetPointer,commandPointer);std::cout<<' ';
                Fields(std::cout,clone->StateForAnalysis(),targetPointer,commandPointer);std::cout<<" 1|\n";continue;
            }
            std::uint32_t random,now,flag,command60,payload,code,present,mutation;
            std::cin>>s.phase>>s.deadline>>now>>random>>flag>>command60>>world.state130>>payload>>code>>present>>mutation;
            Check(bool(std::cin),"complete Minotaur replay input");
            s.target=&world.target;s.randomWord3B4=0x48742400;s.byte3BC=std::uint8_t(flag);s.command=world.command.data();
            world.command[0x60]=std::uint8_t(command60);world.random.state[0]=random;
            world.otherCommand.fill(0xA5);
            if (mutation==1) world.swapOnExitWrite=true;
            else if (mutation==2 || mutation==3) { world.swapOnNotifyWrite=true;world.otherCommand[0x60]=mutation==2?0:255; }
            else if (mutation==4) world.swapOnState=true;
            else Check(mutation==0,"known command-cache mutation input");
            auto time=world.timer.GetStateForAnalysis();time.currentMilliseconds=now;world.timer.SetStateForAnalysis(time);world.Nearest(present!=0);
            Result result=Result::Completed;
            if (operation=="e") result=action.EnterForAnalysis();
            else if (operation=="q") result=action.QueryForAnalysis();
            else if (operation=="x") action.vfunc_34_ClearForAnalysis();
            else if (operation=="u") { Check(action.vfunc_24(),"qualified whole native Update true");result=action.LastResultForAnalysis(); }
            else if (operation=="n")
            {
                std::array<std::uint32_t,8> message{};message[0]=code;reinterpret_cast<std::uint8_t*>(message.data())[0x18]=std::uint8_t(payload);action.vfunc_0C(message.data());
            }
            else throw std::invalid_argument("Minotaur replay operation");
            std::cout<<unsigned(operation=="u")<<' '<<unsigned(result==Result::SelectorRequested)<<' '<<world.selector<<' ';
            Fields(std::cout,s,&world.target,world.command.data(),world.otherCommand.data());std::cout<<' '<<world.random.index;
            const auto& scratch=world.perception.StateForAnalysis().scratchWords30;
            Check(scratch.has_value(),"known Perception replay scratch");
            for (const auto value:*scratch) std::cout<<' '<<value;
            std::cout<<' '<<world.Payload();
            if (mutation)
            {
                constexpr char hex[]="0123456789abcdef";std::cout<<' ';
                for (unsigned i=4;i<world.otherCommand.size();++i) std::cout<<hex[world.otherCommand[i]>>4]<<hex[world.otherCommand[i]&15];
            }
            std::cout<<'|'<<world.effects<<'\n';
        }
    }
    void Contracts()
    {
        World world;wxMinotaurAttackAIAction action(world,Profile::PcCooldown);world.action=&action;action.SetOwnerForAnalysis(&world.owner);
        auto& s=action.StateForAnalysis();Check(!s.word3C4 && s.word3B8==0x3E99999A && !s.byte3BD,"constructor unknown cell and literal defaults");
        for (unsigned phase:{0U,1U,2U,4U,5U})
        { s.phase=phase;world.effects.clear();Check(!action.vfunc_24() && action.LastResultForAnalysis()==Result::UnqualifiedPhase && world.effects.empty(),"five unavailable phases never become successful handlers"); }
        wxMinotaurAttackAIAction guarded(world);guarded.StateForAnalysis().phase=3;
        Check(!guarded.vfunc_24() && guarded.LastResultForAnalysis()==Result::UnqualifiedUpdateProfile,"default profile refuses active Update");
        s.phase=3;s.command=world.command.data();s.byte3BC=1;s.deadline=100;world.command[0x60]=1;world.state130=0x1C;
        auto time=world.timer.GetStateForAnalysis();time.currentMilliseconds=100;world.timer.SetStateForAnalysis(time);world.effects.clear();
        Check(action.vfunc_24() && s.phase==3 && !world.command[0x60] && !world.command[0x20] && !s.byte3BC && !world.random.index,"equality is not expired");
        world.command[0x60]=1;world.command[0x20]=0xA5;world.otherCommand[0x20]=0x5A;
        world.swapOnState=true;world.effects.clear();Check(action.vfunc_24(),"cooldown mutation completes");
        Check(world.command[0x60]==0 && world.otherCommand[0x20]==0 && world.command[0x20]==0xA5,"captured command60 store then fresh command20 cache");
        s.command=world.command.data();world.swapOnExitWrite=true;world.command[0x20]=0xA5;world.otherCommand[0x20]=0x5A;
        action.vfunc_34_ClearForAnalysis();Check(world.command[0x1D]==0 && world.command[0x20]==0xA5 && world.otherCommand[0x20]==0,"Exit reloads cached command between writes");
        s.byte3BC=0;const std::uint32_t shortMessage=0x271C;action.vfunc_0C(&shortMessage); //early return must not read payload18
        World movingWorld;wxMinotaurAttackAIAction moving(movingWorld,Profile::PcCooldownAndFiniteMovement);
        movingWorld.action=&moving;moving.SetOwnerForAnalysis(&movingWorld.owner);
        Check(moving.EnterForAnalysis()==Result::Completed,"movement target from actual common Perception");
        moving.StateForAnalysis().phase=2;movingWorld.Geometry({0,0,0},{0x42C80000,0,0});
        Check(moving.vfunc_24(),"whole admitted phase2 movement");std::uint32_t heading=0;
        std::memcpy(&heading,movingWorld.command.data()+8,4);
        Check(heading==0xBFC90FDB,"original target+x yields negative pi/2: phase2 helper argument order heads away");
        wxMinotaurAttackAIAction::SetFactoryHostForAnalysis(&world);auto factory=spRTTIManager::Instance().Create(wxMinotaurAttackAIAction::ClassID);
        Check(factory && factory->IsKindOf(wxAIAction::ClassID),"actual registered factory identity");wxMinotaurAttackAIAction::SetFactoryHostForAnalysis(nullptr);
    }
}
int main(int argc,char** argv)
{
    try { if (argc==2 && std::string(argv[1])=="--replay") Replay();else { Contracts();std::cout<<"MinotaurAttack bounded contracts passed\n"; }return 0; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
