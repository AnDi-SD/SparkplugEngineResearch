#include "Code/wxFlyingWanderAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxFlyingWanderAIActionAbi.h"
#include "Analysis/PS2/wxFlyingWanderAIActionAbi.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
    struct World final:wxFlyingWanderAIActionHost,wxPerceptionHost
    {
        spNode root;
        int owner=1,target=2;
        std::array<std::uint32_t,3> first{},second{};
        unsigned vectorReads=0,permission=1,selections=0,queries=0;
        wxPerception perception;
        std::string trace;
        std::map<const void*,std::array<float,3>> positions{{&owner,{0,0,0}},{&target,{3,4,0}}};
        World() { perception.BindForAnalysis(*this,&owner,{}); }
        ~World() override { perception.ResetForAnalysis(); }
        void Targets(bool present)
        {
            perception.ResetForAnalysis();perception.BindForAnalysis(*this,&owner,present?std::vector<void*>{&target}:std::vector<void*>{});
            perception.StateForAnalysis().scratchWords30=std::array<std::uint32_t,3>{0xA5A5A5A5,0xA5A5A5A5,0xA5A5A5A5};
        }
        std::array<std::uint32_t,3> ReadDefaultVectorWordsForAnalysis() noexcept override
        { trace+="vector;";return vectorReads++%2==0?first:second; }
        spNode& ReadSceneRootForAnalysis() noexcept override { trace+="root;";return root; }
        std::uint8_t ReadOwnerPermissionForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="permission;";return std::uint8_t(permission); }
        wxPerception& ReadPerceptionForAnalysis(void* actor) noexcept override
        { if (actor!=&owner) std::terminate();trace+="perception;";++queries;return perception; }
        void SelectActionForAnalysis(void* actor,std::uint32_t key,std::uint32_t parameter) noexcept override
        { if (actor!=&owner || key!=2 || parameter!=0) std::terminate();trace+="selector2;";++selections; }
        std::array<float,3> ReadNodePositionForAnalysis(const void* actor) const override { return positions.at(actor); }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
    };
    template<class T> void PrintValues(const T& values)
    { bool first=true;for (const auto value:values) { if (!first) std::cout<<' ';std::cout<<value;first=false; } }
    void Replay()
    {
        std::string op;
        while (std::cin>>op)
        {
            World world;
            if (op=="l")
            {
                unsigned retain;std::cin>>retain;for (auto& value:world.first) std::cin>>value;world.second=world.first;
                auto action=std::make_unique<wxFlyingWanderAIAction>(world);
                const auto words=action->GetOwnWordsForAnalysis();const auto byte=action->GetField3D8ForAnalysis();
                std::weak_ptr<spNode> weak=action->HelperNodeForAnalysis();
                const bool alive=!weak.expired();auto retained=retain?action->HelperNodeForAnalysis():std::shared_ptr<spNode>{};
                auto changed=words;changed[4]=0x12345678;changed[5]=0x87654321;action->SetOwnWordsForAnalysis(changed);
                action->vfunc_30();Check(action->GetOwnWordsForAnalysis()==words,"Entry resets only two pending words");
                Check(action->vfunc_24() && action->vfunc_28() && action->vfunc_2C(),"all three native literal true updates");
                action->vfunc_0C(nullptr);action->vfunc_34_ClearForAnalysis();action.reset();
                Check(world.root.GetChildCountForAnalysis()==0,"lifetime root detached");
                if (retain) Check(!weak.expired() && retained->GetParentForAnalysis()==nullptr,"external retain survives action detach");
                retained.reset();PrintValues(words);std::cout<<' '<<unsigned(byte)<<' '<<unsigned(alive)<<' '
                    <<world.root.GetChildCountForAnalysis()<<' '<<unsigned(weak.expired())<<'\n';
            }
            else if (op=="c")
            {
                unsigned seed;std::cin>>seed;
                auto source=std::make_unique<wxFlyingWanderAIAction>(world);
                auto destination=std::make_unique<wxFlyingWanderAIAction>(world);
                wxFlyingWanderAIAction::OwnWordsForAnalysis words{},other{};
                for (unsigned i=0;i<words.size();++i) { words[i]=seed+i*0x10203U;other[i]=~words[i]; }
                source->SetOwnWordsForAnalysis(words);destination->SetOwnWordsForAnalysis(other);
                source->SetField3D8ForAnalysis(0xA5);destination->SetField3D8ForAnalysis(0x5A);
                spCloneManager manager;Check(source->vfunc_14(*destination,manager),"Copy accepts typed destination");
                auto clone=manager.Clone(*source);auto* typed=dynamic_cast<wxFlyingWanderAIAction*>(clone.get());
                Check(typed && typed->GetOwnWordsForAnalysis()==words,"Clone copies twelve words");
                const auto result=destination->GetOwnWordsForAnalysis();const auto byte=destination->GetField3D8ForAnalysis();
                const auto cloneByte=typed->GetField3D8ForAnalysis();
                const bool distinct=source->HelperNodeForAnalysis()!=destination->HelperNodeForAnalysis() &&
                    typed->HelperNodeForAnalysis()!=source->HelperNodeForAnalysis() &&
                    typed->HelperNodeForAnalysis()!=destination->HelperNodeForAnalysis();
                clone.reset();destination.reset();source.reset();
                PrintValues(result);std::cout<<' '<<unsigned(byte)<<' '<<unsigned(cloneByte)<<' '
                    <<unsigned(distinct)<<' '<<world.root.GetChildCountForAnalysis()<<'\n';
            }
            else if (op=="q")
            {
                unsigned permission,targets;std::cin>>permission>>targets;
                world.permission=permission;world.Targets(targets!=0);
                wxFlyingWanderAIAction action(world);action.SetOwnerForAnalysis(&world.owner);action.vfunc_38();
                std::cout<<world.selections<<' ';PrintValues(world.perception.StateForAnalysis().scratchWords30.value());std::cout<<'\n';
            }
            else if (op=="v")
            {
                for (auto& value:world.first) std::cin>>value;for (auto& value:world.second) std::cin>>value;
                auto action=std::make_unique<wxFlyingWanderAIAction>(world);
                const auto words=action->GetOwnWordsForAnalysis();const auto byte=action->GetField3D8ForAnalysis();
                std::weak_ptr<spNode> weak=action->HelperNodeForAnalysis();action.reset();
                PrintValues(words);std::cout<<' '<<unsigned(byte)<<' '<<world.root.GetChildCountForAnalysis()<<' '
                    <<unsigned(weak.expired())<<'\n';
            }
            else throw std::invalid_argument("FlyingWander replay operation");
            Check(bool(std::cin),"complete FlyingWander input");
        }
    }
    void OwnershipAndCopy()
    {
        World world;world.first={0x80000000,0x3F800000,0x40E00000};world.second={1,2,3};
        std::weak_ptr<spNode> weak;std::shared_ptr<spNode> retained;
        {
            wxFlyingWanderAIAction source(world);weak=source.HelperNodeForAnalysis();
            Check(world.trace=="vector;vector;root;" && world.vectorReads==2,"two raw vector reads precede root attach");
            Check(world.root.GetChildCountForAnalysis()==1 && source.HelperNodeForAnalysis()->GetParentForAnalysis()==&world.root,
                "actual shared Node reciprocal root ownership");
            Check(source.GetOwnWordsForAnalysis()[0]==0x80000000 && source.GetOwnWordsForAnalysis()[6]==1 &&
                source.GetOwnWordsForAnalysis()[3]==0x43960000,"raw vector triplets and300f default");
            Check(source.HelperNodeForAnalysis()->GetPositionForAnalysis()==spNode::Vector3{0,0,0} &&
                source.HelperNodeForAnalysis()->GetFlagsForAnalysis()==(spNode::NativeDefaultFlags|5),
                "action raw default vector is not the Node constructor default; common Attach marks flags");
            source.SetField3D8ForAnalysis(0xA5);source.SetOwnerForAnalysis(&world.owner);
            wxFlyingWanderAIAction destination(world);auto destinationNode=destination.HelperNodeForAnalysis();
            destination.SetField3D8ForAnalysis(0x5A);spCloneManager manager;
            Check(source.vfunc_14(destination,manager) && destination.GetOwnWordsForAnalysis()==source.GetOwnWordsForAnalysis() &&
                destination.HelperNodeForAnalysis()==destinationNode && destination.GetField3D8ForAnalysis()==0x5A,
                "Copy transfers twelve fields and preserves destination byte and owned Node");
            auto clone=manager.Clone(source);auto* flying=dynamic_cast<wxFlyingWanderAIAction*>(clone.get());
            Check(flying && flying->GetField3D8ForAnalysis()==0 && flying->HelperNodeForAnalysis()!=source.HelperNodeForAnalysis() &&
                flying->HelperNodeForAnalysis()!=destinationNode && flying->GetOwnerForAnalysis()==nullptr,
                "Clone gets a fresh attached Node and constructor byte while base owner stays default");
            retained=source.HelperNodeForAnalysis();
        }
        Check(world.root.GetChildCountForAnalysis()==0 && !weak.expired() && retained->GetParentForAnalysis()==nullptr,
            "all action destructors detach before dropping own references, external retained Node survives");
        retained.reset();Check(weak.expired(),"last external retained Node release finishes lifetime");
        wxFlyingWanderAIAction::SetFactoryHostForAnalysis(&world);
        auto factory=spRTTIManager::Instance().Create(wxFlyingWanderAIAction::ClassID);
        Check(factory && factory->IsKindOf(wxAIAction::ClassID),"registered factory and actual physical base identity");
        factory.reset();wxFlyingWanderAIAction::SetFactoryHostForAnalysis(nullptr);
    }
    void HooksAndQuery()
    {
        World world;world.Targets(true);wxFlyingWanderAIAction action(world);action.SetOwnerForAnalysis(&world.owner);
        struct Child final:wxAIAction { unsigned calls=0;void vfunc_0C(const void*) noexcept override { ++calls; } } child;
        action.SetCurrentActionForAnalysis(&child);action.vfunc_0C(nullptr);Check(child.calls==0,"Notify no forwarding");
        action.SetCurrentActionForAnalysis(nullptr);action.SetClearableWordForAnalysis(17);action.vfunc_34_ClearForAnalysis();
        Check(action.GetClearableWordForAnalysis()==17,"Exit is literal no-op, not common clear");
        auto words=action.GetOwnWordsForAnalysis();words[4]=99;words[5]=77;action.SetOwnWordsForAnalysis(words);
        action.vfunc_30();Check(action.GetOwnWordsForAnalysis()[4]==0 && action.GetOwnWordsForAnalysis()[5]==0,"Entry resets both own pending words");
        world.permission=0;world.trace.clear();Check(action.QueryForAnalysis()==wxFlyingWanderQueryResultForAnalysis::PermissionDisabled &&
            world.trace=="permission;" && world.queries==0,"disabled permission skips actual Perception");
        world.permission=255;world.trace.clear();Check(action.QueryForAnalysis()==wxFlyingWanderQueryResultForAnalysis::SelectorRequested &&
            world.trace=="permission;perception;selector2;" && world.selections==1,"nonzero permission uses real shared nearest and selector2 entry");
        world.Targets(false);Check(action.QueryForAnalysis()==wxFlyingWanderQueryResultForAnalysis::NoTarget && world.selections==1,
            "empty nearest leaves action selected");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if (argc==2 && std::string(argv[1])=="--replay") Replay();
        else { OwnershipAndCopy();HooksAndQuery();std::cout<<"wxFlyingWanderAIAction checks passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
