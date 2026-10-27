#include "Code/wxIceGargoyleClawAIAction.h"
#include "Code/wxIceGargoyleWithdrawlAIAction.h"
#include "Code/wxTargetManager.h"
#include "Code/wxAssetManager.h"
#include "Code/wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxIceGargoyleClawAIActionAbi.h"
#include "Analysis/PC/wxIceGargoyleWithdrawlAIActionAbi.h"
#include "Analysis/PS2/wxIceGargoyleClawAIActionAbi.h"
#include "Analysis/PS2/wxIceGargoyleWithdrawlAIActionAbi.h"
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    using Result=wxIceGargoyleResultForAnalysis;
    using Profile=wxIceGargoyleUpdateProfileForAnalysis;
    void Check(bool value,const char* label) { if (!value) throw std::runtime_error(label); }
    std::uint32_t Word(float value) { return std::bit_cast<std::uint32_t>(value); }
    struct AssetHost final:wxAssetManagerHost
    {
        bool IsPs2ForAnalysis() const noexcept override { return false; }
        std::uint32_t AppLanguageForAnalysis() const noexcept override { return 0; }
        void SetAppLanguageForAnalysis(std::uint32_t) noexcept override {}
        std::optional<std::string> ReadPcRegistryValueForAnalysis(std::string_view,std::string_view key) override
        { return key=="MediaPath"?std::optional<std::string>{"A/"}:std::nullopt; }
        std::unique_ptr<wxAssetFileStreamForAnalysis> CreateFileStreamForAnalysis() override { return {}; }
        void ReportOpenFailureForAnalysis(std::string_view) noexcept override {}
    };
    struct TargetServices final:wxTargetManagerHost
    {
        AssetHost assetHost;
        wxAssetManager asset{assetHost};
        spResourceManager resource;
        std::shared_ptr<spNode> root=std::make_shared<spNode>();
        std::shared_ptr<wxTargetNodeBindingForAnalysis> arrow=std::make_shared<wxTargetNodeBindingForAnalysis>();
        TargetServices() { asset.InitializeForAnalysis();arrow->node=std::make_shared<spNode>(); }
        wxAssetManager* ResolveTargetAssetManagerForAnalysis() override { return &asset; }
        spResourceManager* ResolveTargetResourceManagerForAnalysis() override { return &resource; }
        std::shared_ptr<wxTargetNodeBindingForAnalysis> LoadTargetNodeForAnalysis(spResourceManager& service,const char* path) override
        { Check(&service==&resource && std::string(path)=="A/SFX\\arow.smo","real AssetManager path and foreign resource input");return arrow; }
        spNode* ResolveTargetSceneRootForAnalysis() override { return root.get(); }
    };
    struct World final:wxIceGargoyleAIActionHost,wxPerceptionHost
    {
        int owner=1,targetIdentity=2,secondOwner=3;
        std::array<std::uint8_t,0x74> command{},otherCommand{};
        void* currentCommand=command.data();
        bool swapAfterFirstRead=false,recordKnown=true;
        spNode ownerNode;
        std::shared_ptr<spNode> targetNode=std::make_shared<spNode>();
        spTimer clock;
        wxGameTimer timer{clock};
        wxPerception perception;
        std::uint32_t state=0,lower=Word(-100),upper=Word(200);
        wxTargetCandidateBindingForAnalysis record{1};
        spNode* recordNode=targetNode.get();
        TargetServices targetServices;
        std::unique_ptr<wxTargetManager> manager;
        std::vector<std::array<std::uint32_t,4>> effects;
        std::vector<std::uint32_t> selections;
        std::vector<std::uint32_t> selectionOwners;
        wxAIAction* mutationAction=nullptr;
        std::string mutationTrigger;
        mutable bool mutationDone=false;
        World()
        {
            command.fill(0xA5);otherCommand.fill(0xA5);
            ownerNode.SetPositionForAnalysis({111,222,333});ownerNode.MarkLocalTransformDirtyForAnalysis();
            Check(ownerNode.UpdateWorldForAnalysis(),"real owner cache setup");
            Geometry({0,0,0},{0,150,100});
        }
        ~World() override { perception.ResetForAnalysis(); }
        void Rebind(std::string_view trigger) const noexcept
        {
            if (mutationAction && !mutationDone && trigger==mutationTrigger)
            { mutationAction->SetOwnerForAnalysis(const_cast<int*>(&secondOwner));mutationDone=true; }
        }
        bool KnownOwner(const void* actor) const noexcept { return actor==&owner || actor==&secondOwner; }
        void Geometry(const spNode::Vector3& origin,const spNode::Vector3& target)
        {
            ownerNode.SetPositionForAnalysis(origin); //local20, distinct cached74
            targetNode->SetPositionForAnalysis(target);targetNode->MarkLocalTransformDirtyForAnalysis();
            Check(targetNode->UpdateWorldForAnalysis(),"real target cached world74");
            targetNode->SetPositionForAnalysis({-111,-222,-333}); //different local20
        }
        void Manager(unsigned mode)
        {
            manager=std::make_unique<wxTargetManager>(targetServices);
            recordNode=mode==2?targetNode.get():nullptr;
            if (mode) manager->RegisterCandidateForAnalysis(record);
            Check(manager->CurrentWordsForAnalysis()[2]==(mode?reinterpret_cast<std::uintptr_t>(&record):0),"real manager current28 producer");
        }
        void Nearest(bool present)
        {
            perception.ResetForAnalysis();
            perception.BindForAnalysis(*this,&owner,present?std::vector<void*>{&targetIdentity}:std::vector<void*>{});
            perception.StateForAnalysis().scratchWords30=std::array<std::uint32_t,3>{0xA5A5A5A5,0xA5A5A5A5,0xA5A5A5A5};
        }
        spNode& ReadOwnerNodeForAnalysis(void* actor) noexcept override
        { if (!KnownOwner(actor)) std::terminate();return ownerNode; }
        void* ReadCommandForAnalysis(void* actor) noexcept override
        {
            if (!KnownOwner(actor)) std::terminate();
            if (actor==&secondOwner) return otherCommand.data();
            auto* result=currentCommand;
            // Explicit changing borrowed-data input. Native counterpart swaps
            // character130 in the foreign CRT interval after the first read.
            if (swapAfterFirstRead) { currentCommand=otherCommand.data();swapAfterFirstRead=false; }
            return result;
        }
        unsigned CommandID(void* value) const noexcept
        { if (value==command.data()) return 0;if (value==otherCommand.data()) return 1;std::terminate(); }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        {
            const auto id=CommandID(value);if (offset!=4 && offset!=8) std::terminate();
            std::memcpy(static_cast<std::uint8_t*>(value)+offset,&word,4);effects.push_back({id,offset,4,word});
            if (id==0 && offset==8) { Rebind("aim-heading");if (word==0x40490FDB) Rebind("claw-heading"); }
            if (id==0 && offset==4 && word==0x3F800000) Rebind("withdrawl-motion");
        }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {
            const auto id=CommandID(value);if (offset!=0x20 && offset!=0x21) std::terminate();
            static_cast<std::uint8_t*>(value)[offset]=byte;effects.push_back({id,offset,1,byte});
            if (id==0 && offset==0x20 && byte==0) Rebind("exit-first");
        }
        std::uint32_t ReadCharacterFlags140ForAnalysis(void* actor) noexcept override
        { if (!KnownOwner(actor)) std::terminate();return actor==&secondOwner?0x400:state; }
        std::uint32_t ReadOwnerWordForAnalysis(void* actor,std::uint32_t offset) noexcept override
        {
            if (!KnownOwner(actor) || (offset!=0x1CC && offset!=0x1B4)) std::terminate();
            const auto value=offset==0x1CC?lower:upper;Rebind(offset==0x1CC?"bound-lower":"bound-upper");return value;
        }
        std::optional<spNode*> ReadBorrowedRecordNode18ForAnalysis(std::uintptr_t identity) noexcept override
        { if (identity!=reinterpret_cast<std::uintptr_t>(&record)) std::terminate();Rebind("record-node");return recordKnown?std::optional<spNode*>{recordNode}:std::nullopt; }
        wxPerception& ReadPerceptionForAnalysis(void* actor) noexcept override
        { if (!KnownOwner(actor)) std::terminate();return perception; }
        wxGameTimer& GameTimerForAnalysis() noexcept override { return timer; }
        void SelectActionForAnalysis(void* actor,std::uint32_t key,std::uint32_t parameter) noexcept override
        { if (!KnownOwner(actor) || parameter || (key!=0 && key!=2)) std::terminate();selections.push_back(key);selectionOwners.push_back(actor==&secondOwner?1:0); }
        std::array<float,3> ReadNodePositionForAnalysis(const void* actor) const override
        { if (KnownOwner(actor)) return ownerNode.GetPositionForAnalysis();Check(actor==&targetIdentity,"real Perception target identity");const auto value=targetNode->GetPositionForAnalysis();Rebind("nearest-target");return value; }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
        std::uint32_t CommandWord(bool other,unsigned offset) const
        { std::uint32_t value;std::memcpy(&value,(other?otherCommand:command).data()+offset,4);return value; }
        void GuardCommands() const
        {
            for (const auto* c:{&command,&otherCommand})
                for (unsigned i=0;i<c->size();++i)
                    if (!(i>=4 && i<12) && i!=0x20 && i!=0x21) Check((*c)[i]==0xA5,"all unrelated command bytes unchanged");
        }
    };
    std::unique_ptr<wxAIAction> Create(char kind,World& world,Profile profile=Profile::PcFiniteGeometry)
    {
        std::unique_ptr<wxAIAction> value;
        if (kind=='c') value=std::make_unique<wxIceGargoyleClawAIAction>(world,profile);
        else { Check(kind=='w',"known physical sibling");value=std::make_unique<wxIceGargoyleWithdrawlAIAction>(world,profile); }
        value->SetOwnerForAnalysis(&world.owner);return value;
    }
    wxIceGargoyleAIActionStateForAnalysis& State(wxAIAction& action)
    {
        if (auto* claw=dynamic_cast<wxIceGargoyleClawAIAction*>(&action)) return claw->StateForAnalysis();
        return dynamic_cast<wxIceGargoyleWithdrawlAIAction&>(action).StateForAnalysis();
    }
    Result Enter(wxAIAction& action)
    {
        if (auto* claw=dynamic_cast<wxIceGargoyleClawAIAction*>(&action)) return claw->EnterForAnalysis();
        return dynamic_cast<wxIceGargoyleWithdrawlAIAction&>(action).EnterForAnalysis();
    }
    Result Query(wxAIAction& action)
    {
        if (auto* claw=dynamic_cast<wxIceGargoyleClawAIAction*>(&action)) return claw->QueryForAnalysis();
        return dynamic_cast<wxIceGargoyleWithdrawlAIAction&>(action).QueryForAnalysis();
    }
    void Output(wxAIAction& action,const World& world)
    {
        const auto& s=State(action);
        std::cout<<s.word3A8<<' '<<unsigned(s.cachedNode==world.targetNode.get())<<' '<<s.phase<<' '<<s.stamp<<' '
            <<world.CommandWord(false,4)<<' '<<world.CommandWord(false,8)<<' '<<+world.command[0x20]<<' '<<+world.command[0x21]<<' '
            <<world.CommandWord(true,4)<<' '<<world.CommandWord(true,8);
    }
    void Effects(const World& world)
    {
        world.GuardCommands();std::cout<<'|';
        for (const auto& e:world.effects) std::cout<<e[0]<<','<<e[1]<<','<<e[2]<<','<<e[3]<<';';
        std::cout<<'\n';
    }
    void MutationReplay(const std::string& name,World& world)
    {
        const char kind=name.find("withdrawl")!=std::string::npos?'w':'c';
        auto action=Create(kind,world);auto& s=State(*action);s.cachedNode=world.targetNode.get();s.phase=7;s.stamp=123;
        world.mutationAction=action.get();bool returned=false;
        if (name=="aim-claw" || name=="aim-withdrawl" || name=="stage0-claw")
        {
            world.mutationTrigger="aim-heading";if (name=="stage0-claw") s.phase=0;
            returned=action->vfunc_24();
        }
        else if (name=="exit-claw" || name=="exit-withdrawl")
        { world.mutationTrigger="exit-first";action->vfunc_34_ClearForAnalysis(); }
        else if (name=="query-claw" || name=="query-withdrawl")
        {
            world.Nearest(true);world.targetNode->SetPositionForAnalysis({3.0e19f,0,0});world.mutationTrigger="nearest-target";
            Check(Query(*action)==Result::SelectorRequested,"finite non-nearest query reaches selector");
        }
        else if (name=="stage-withdrawl")
        { s.phase=1;world.Geometry({0,0,0},{3,0,4});world.mutationTrigger="withdrawl-motion";returned=action->vfunc_24(); }
        else if (name=="stage-claw")
        { s.phase=1;world.Geometry({0,0,0},{0,0,100});world.mutationTrigger="claw-heading";returned=action->vfunc_24(); }
        else if (name=="bounds-claw-below" || name=="bounds-claw-above")
        {
            const bool below=name=="bounds-claw-below";world.lower=Word(below?200.0f:-100.0f);world.upper=Word(below?300.0f:100.0f);
            world.mutationTrigger=below?"bound-lower":"bound-upper";
            Check(dynamic_cast<wxIceGargoyleClawAIAction&>(*action).CheckBoundsForAnalysis()==Result::SelectorRequested,"bound mutation selector");
        }
        else if (name=="entry-claw")
        { world.Manager(1);world.mutationTrigger="record-node";Check(Enter(*action)==Result::SelectorRequested,"entry known null Node selector"); }
        else throw std::runtime_error("unknown mutation input");
        Check(world.mutationDone,"explicit borrowed owner mutation was consumed");Output(*action,world);
        std::cout<<' '<<unsigned(action->GetOwnerForAnalysis()==&world.secondOwner)<<' '
            <<(world.selectionOwners.empty()?0xFFFFFFFFU:world.selectionOwners.back())<<' '
            <<(world.selections.empty()?0xFFFFFFFFU:world.selections.back())<<' '<<unsigned(returned);
        Effects(world);
    }
    void Replay()
    {
        std::string op;
        while (std::cin>>op)
        {
            World world;
            if (op=="m") { std::string name;std::cin>>name;MutationReplay(name,world);continue; }
            if (op=="b")
            {
                std::uint32_t target;std::cin>>world.lower>>target>>world.upper;
                world.Geometry({0,0,0},{0,std::bit_cast<float>(target),100});
                wxIceGargoyleClawAIAction action(world);action.SetOwnerForAnalysis(&world.owner);
                action.StateForAnalysis().cachedNode=world.targetNode.get();action.StateForAnalysis().phase=7;action.StateForAnalysis().stamp=123;
                Check(action.vfunc_28(),"qualified bounds literal true or selector boundary");
                std::cout<<unsigned(!world.selections.empty())<<' ';Output(action,world);Effects(world);continue;
            }
            char kind;std::cin>>kind;auto action=Create(kind,world);auto& s=State(*action);
            if (op=="l")
            {
                std::cout<<s.word3A8<<' '<<unsigned(s.cachedNode!=nullptr)<<' '<<s.phase<<' '<<s.stamp<<' ';
                s.word3A8=0x41234567;s.cachedNode=world.targetNode.get();s.phase=7;s.stamp=0x12345678;
                auto destination=Create(kind,world);spCloneManager manager;
                Check(action->vfunc_14(*destination,manager),"same-kind own Copy");auto clone=action->Clone();
                auto* cloned=dynamic_cast<wxAIAction*>(clone.get());Check(cloned,"actual derived Clone");
                for (auto* value:{destination.get(),cloned})
                { const auto& t=State(*value);Check(t.word3A8==s.word3A8 && t.cachedNode==s.cachedNode && t.phase==s.phase && t.stamp==s.stamp,"exact own four-field Copy"); }
                action->vfunc_0C(nullptr);Check(action->vfunc_2C(),"literal common true slot");
                std::cout<<s.word3A8<<' '<<unsigned(s.cachedNode==world.targetNode.get())<<' '<<s.phase<<' '<<s.stamp<<' ';
                clone.reset();destination.reset();action.reset();
                Check(world.targetNode.use_count()==1,"borrowed Node never retained by Copy/Clone/destruction");
                std::cout<<"1 1 1";Effects(world);continue;
            }
            s.cachedNode=world.targetNode.get();
            if (op=="u")
            {
                unsigned swap;std::uint32_t now;std::array<std::uint32_t,3> origin{},target{};
                std::cin>>s.phase>>s.stamp>>now>>world.state;
                for (auto& value:origin) std::cin>>value;for (auto& value:target) std::cin>>value;std::cin>>swap;
                world.Geometry({std::bit_cast<float>(origin[0]),std::bit_cast<float>(origin[1]),std::bit_cast<float>(origin[2])},
                    {std::bit_cast<float>(target[0]),std::bit_cast<float>(target[1]),std::bit_cast<float>(target[2])});
                auto clock=world.timer.GetStateForAnalysis();clock.currentMilliseconds=now;world.timer.SetStateForAnalysis(clock);
                world.swapAfterFirstRead=swap!=0;const bool returned=action->vfunc_24();Output(*action,world);std::cout<<' '<<unsigned(returned);
            }
            else
            {
                s.phase=7;s.stamp=123;
                if (op=="e") { unsigned mode;std::cin>>mode;world.Manager(mode);s.stamp=999;const auto result=Enter(*action);std::cout<<unsigned(result==Result::SelectorRequested)<<' '; }
                else if (op=="q") { unsigned present;std::cin>>present;world.Nearest(present!=0);std::cout<<unsigned(Query(*action)==Result::SelectorRequested)<<' '; }
                else if (op=="x") { action->vfunc_34_ClearForAnalysis();std::cout<<"0 "; }
                else if (op=="g") { Check(Enter(*action)==Result::UnqualifiedColdTargetManager,"cold manager is explicit prefix guard");std::cout<<"1 "; }
                else throw std::runtime_error("unknown replay operation");
                Output(*action,world);
                if (op=="q") for (const auto value:*world.perception.StateForAnalysis().scratchWords30) std::cout<<' '<<value;
            }
            Check(bool(std::cin),"complete native replay input");Effects(world);
        }
    }
    void Guards()
    {
        World world;auto action=Create('c',world,Profile::HooksOnly);
        Check(action->IsExactly(wxIceGargoyleClawAIAction::ClassID) && action->IsKindOf(wxAIAction::ClassID),"physical/registration base identity");
        State(*action).cachedNode=world.targetNode.get();Check(!action->vfunc_24() && world.effects.empty(),"default Update refusal has no writes");
        Check(Enter(*action)==Result::UnqualifiedColdTargetManager && State(*action).cachedNode==world.targetNode.get(),"cold entry preserves own fields");
        world.Manager(2);world.recordKnown=false;
        Check(Enter(*action)==Result::UnqualifiedTargetRecordData && State(*action).cachedNode==world.targetNode.get(),"unknown record data is not a null Node");
        auto active=Create('w',world);State(*active).cachedNode=world.targetNode.get();
        world.Geometry({0,0,0},{2048,0,1});Check(!active->vfunc_24() && world.effects.empty(),"unqualified numeric geometry has explicit refusal");
        spBaseObject unrelated;spCloneManager manager;Check(!active->vfunc_14(unrelated,manager),"unsafe destination tail is not accessed");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if (argc==2 && std::string(argv[1])=="--replay") Replay();
        else { Guards();std::cout<<"IceGargoyle paired bounded contracts PASS\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
