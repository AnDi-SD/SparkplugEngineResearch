#include "Code/wxIceGargoyleAttackAIAction.h"
#include "Code/wxTargetManager.h"
#include "Code/wxAssetManager.h"
#include "Code/wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxIceGargoyleAttackAIActionAbi.h"
#include "Analysis/PS2/wxIceGargoyleAttackAIActionAbi.h"
#include <bit>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    using Profile=wxIceGargoyleAttackProfileForAnalysis;
    using Result=wxIceGargoyleAttackResultForAnalysis;
    void Check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
    std::uint32_t Word(float value) { return std::bit_cast<std::uint32_t>(value); }
    spNode::Vector3 Floats(const std::array<std::uint32_t,3>& words)
    { return {std::bit_cast<float>(words[0]),std::bit_cast<float>(words[1]),std::bit_cast<float>(words[2])}; }
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
        AssetHost assetHost;wxAssetManager asset{assetHost};spResourceManager resource;
        std::shared_ptr<spNode> root=std::make_shared<spNode>();
        std::shared_ptr<wxTargetNodeBindingForAnalysis> arrow=std::make_shared<wxTargetNodeBindingForAnalysis>();
        TargetServices() { asset.InitializeForAnalysis();arrow->node=std::make_shared<spNode>(); }
        wxAssetManager* ResolveTargetAssetManagerForAnalysis() override { return &asset; }
        spResourceManager* ResolveTargetResourceManagerForAnalysis() override { return &resource; }
        std::shared_ptr<wxTargetNodeBindingForAnalysis> LoadTargetNodeForAnalysis(spResourceManager& service,const char* path) override
        { Check(&service==&resource && std::string(path)=="A/SFX\\arow.smo","actual shared resource path");return arrow; }
        spNode* ResolveTargetSceneRootForAnalysis() override { return root.get(); }
    };
    struct World final:wxIceGargoyleAttackAIActionHost,wxPerceptionHost
    {
        int owner=1,targetIdentity=2;
        std::shared_ptr<spNode> ownerNode=std::make_shared<spNode>(),targetNode=std::make_shared<spNode>();
        std::array<std::uint8_t,0x74> command{};
        spTimer clock;wxGameTimer timer{clock};wxPerception perception;
        spFunctionEval::RandomStateForAnalysis random;
        TargetServices services;std::unique_ptr<wxTargetManager> manager;
        wxTargetCandidateBindingForAnalysis record{1};
        bool recordKnown=true,recordHasNode=true,brainPresent=false;
        std::uint32_t state130=0,state140=0,lower=Word(-100),upper=Word(200);
        std::vector<std::uint32_t> selections;
        std::string effects;
        World()
        {
            command.fill(0xA5);random.state.fill(0);random.index=0;
            ownerNode->SetPositionForAnalysis({111,222,333});ownerNode->MarkLocalTransformDirtyForAnalysis();
            Check(ownerNode->UpdateWorldForAnalysis(),"real cached owner pose");
            Geometry({0,0,0},{0,150,100});
        }
        ~World() override { perception.ResetForAnalysis(); }
        void Geometry(const spNode::Vector3& origin,const spNode::Vector3& target)
        {
            ownerNode->SetPositionForAnalysis(origin);
            targetNode->SetPositionForAnalysis(target);targetNode->MarkLocalTransformDirtyForAnalysis();
            Check(targetNode->UpdateWorldForAnalysis(),"actual cached target WORLD");targetNode->SetPositionForAnalysis({-111,-222,-333});
        }
        void Manager(unsigned mode=2)
        {
            manager=std::make_unique<wxTargetManager>(services);recordHasNode=mode==2;
            if (mode) manager->RegisterCandidateForAnalysis(record);
        }
        void Nearest(bool present)
        {
            perception.ResetForAnalysis();perception.BindForAnalysis(*this,&owner,present?std::vector<void*>{&targetIdentity}:std::vector<void*>{});
            perception.StateForAnalysis().scratchWords30=std::array<std::uint32_t,3>{0xA5A5A5A5,0xA5A5A5A5,0xA5A5A5A5};
        }
        spNode& ReadOwnerNodeForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return *ownerNode; }
        void* ReadCommandForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return command.data(); }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        {
            if (value!=command.data() || offset+4>command.size()) std::terminate();std::memcpy(command.data()+offset,&word,4);
            effects+=std::to_string(offset)+",4,"+std::to_string(word)+";";
        }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {
            if (value!=command.data() || offset>=command.size()) std::terminate();command[offset]=byte;
            effects+=std::to_string(offset)+",1,"+std::to_string(byte)+";";
        }
        void ResetCommandForAnalysis(void* value) noexcept override
        {
            if (value!=command.data()) std::terminate();effects+="R;";
            // Prior constructor/vtable-qualified foreign5992E0 payload, not
            // an owned action operation or a substitute RNG/geometry body.
            for (const unsigned off:{4U,8U,12U,16U,20U,0x6CU}) std::memset(command.data()+off,0,4);
            for (unsigned i=0x19;i<=0x31;++i) command[i]=0;
            for (unsigned i=0x5C;i<=0x60;++i) command[i]=0;
            command[0x64]=command[0x70]=0;
        }
        std::uint32_t ReadCharacterFlags140ForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return state140; }
        std::uint32_t ReadCharacterState130ForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return state130; }
        void* ReadCharacterBrain138ForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return brainPresent?this:nullptr; }
        std::uint32_t ReadOwnerWordForAnalysis(void* value,std::uint32_t offset) noexcept override
        { if (value!=&owner || (offset!=0x1B4 && offset!=0x1CC)) std::terminate();return offset==0x1B4?upper:lower; }
        std::optional<spNode*> ReadBorrowedRecordNode18ForAnalysis(std::uintptr_t identity) noexcept override
        { if (identity!=reinterpret_cast<std::uintptr_t>(&record)) std::terminate();return recordKnown?std::optional<spNode*>{recordHasNode?targetNode.get():nullptr}:std::nullopt; }
        wxPerception& ReadPerceptionForAnalysis(void* value) noexcept override { if (value!=&owner) std::terminate();return perception; }
        wxGameTimer& GameTimerForAnalysis() noexcept override { return timer; }
        spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept override { return random; }
        void SelectActionForAnalysis(void* value,std::uint32_t key,std::uint32_t parameter) noexcept override
        { if (value!=&owner || parameter || (key!=0 && key!=5)) std::terminate();selections.push_back(key); }
        std::array<std::uint32_t,3> ReadDefaultVectorWordsForAnalysis() noexcept override { return {}; }
        std::array<std::uint32_t,3> ReadUpVectorWordsForAnalysis() noexcept override { return {0,0x3F800000,0}; }
        std::array<std::uint32_t,3> ReadWaypointWordsForAnalysis(unsigned index) noexcept override
        {
            static constexpr std::array<spNode::Vector3,4> values{{{1700,1350,1070},{1700,1350,888},{1700,1350,635},{1700,1350,413}}};
            if (index>=4) std::terminate();const auto& p=values[index];return {Word(p[0]),Word(p[1]),Word(p[2])};
        }
        std::array<float,3> ReadNodePositionForAnalysis(const void* value) const override
        { if (value==&owner) return ownerNode->GetPositionForAnalysis();Check(value==&targetIdentity,"borrowed Perception identity");return targetNode->GetPositionForAnalysis(); }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
        std::string Payload() const
        {
            constexpr char hex[]="0123456789abcdef";std::string result;
            for (unsigned i=4;i<command.size();++i) { result+=hex[command[i]>>4];result+=hex[command[i]&15]; }
            return result;
        }
    };
    void Own(const wxIceGargoyleAttackStateForAnalysis& s,const World& w)
    {
        for (auto value:s.waypoint) std::cout<<value<<' ';for (auto value:s.direction) std::cout<<value<<' ';
        std::cout<<s.speed.value_or(0)<<' '<<s.word3C8<<' '<<s.word3CC<<' '<<s.threshold<<' '
            <<unsigned(s.cachedNode==w.targetNode.get())<<' '<<s.phase<<' '<<s.time<<' '
            <<+s.toggle<<' '<<+s.byte3D0<<' '<<+s.byte3D1<<' '<<+s.byte3D2<<' '<<+s.latch<<' '<<unsigned(s.speed.has_value())<<' ';
    }
    void Snapshot(wxIceGargoyleAttackAIAction& action,const World& w,unsigned returned)
    {
        std::cout<<returned<<' '<<unsigned(!w.selections.empty())<<' '<<(w.selections.empty()?0:w.selections.back())<<' ';Own(action.StateForAnalysis(),w);
        for (const auto value:w.ownerNode->GetPositionForAnalysis()) std::cout<<Word(value)<<' ';
        for (const auto value:w.ownerNode->GetWorldPositionForAnalysis()) std::cout<<Word(value)<<' ';
        for (const auto value:w.ownerNode->GetOrientationForAnalysis()) std::cout<<Word(value)<<' ';
        for (const auto value:w.ownerNode->GetWorldOrientationForAnalysis()) std::cout<<Word(value)<<' ';
        std::cout<<w.ownerNode->GetFlagsForAnalysis()<<' '<<w.random.index<<' ';
        for (const auto value:*w.perception.StateForAnalysis().scratchWords30) std::cout<<value<<' ';
        std::cout<<w.Payload()<<'|'<<w.effects<<'\n';
    }
    void Lifecycle(std::uint32_t rawTarget,World& w)
    {
        wxIceGargoyleAttackAIAction source(w,Profile::PcFiniteGeometry),destination(w);Own(source.StateForAnalysis(),w);
        auto value=[](std::uint32_t off){return 0x12345678U+off*0x10203U;};auto& s=source.StateForAnalysis();auto& d=destination.StateForAnalysis();
        for (unsigned axis=0;axis!=3;++axis)
        {
            s.waypoint[axis]=value(0x3A8+4*axis);d.waypoint[axis]=~s.waypoint[axis];
            s.direction[axis]=value(0x3B8+4*axis);d.direction[axis]=~s.direction[axis];
        }
        s.speed=value(0x3C4);d.speed=~*s.speed;s.word3C8=value(0x3C8);d.word3C8=~s.word3C8;s.word3CC=value(0x3CC);d.word3CC=~s.word3CC;
        s.threshold=value(0x3D4);d.threshold=~s.threshold;s.cachedNode=w.targetNode.get();
        // Native input is deliberately a32-bit word. Complete the complement
        // at that width before the explicit portable pointer representation.
        const std::uint32_t oldDestinationWord=rawTarget^0xFFFFFFFFU;
        d.cachedNode=reinterpret_cast<spNode*>(static_cast<std::uintptr_t>(oldDestinationWord)); //never dereferenced
        s.phase=value(0x3DC);d.phase=~s.phase;s.time=value(0x3E0);d.time=~s.time;
        s.toggle=s.byte3D0=s.byte3D1=s.byte3D2=s.latch=0xA5;d.toggle=d.byte3D0=d.byte3D1=d.byte3D2=d.latch=0x5A;
        spCloneManager manager;Check(source.vfunc_14(destination,manager),"same-kind Copy");Own(d,w);
        auto clone=source.Clone();auto* typed=dynamic_cast<wxIceGargoyleAttackAIAction*>(clone.get());Check(typed,"actual own Clone type");Own(typed->StateForAnalysis(),w);
        clone.reset();Check(w.targetNode.use_count()==1,"all cached Node fields are borrowed");std::cout<<"1|\n";
    }
    void Replay()
    {
        std::string op;
        while (std::cin>>op)
        {
            World world;
            if (op=="l") { std::uint32_t raw;std::cin>>raw;Lifecycle(raw,world);continue; }
            wxIceGargoyleAttackAIAction action(world,Profile::PcFiniteGeometry);action.SetOwnerForAnalysis(&world.owner);auto& s=action.StateForAnalysis();
            std::uint32_t now,dt,speedKnown,speed;unsigned toggle,latch;std::array<std::uint32_t,3> origin{},target{};
            std::cin>>s.phase>>s.time>>now>>dt>>speedKnown>>speed>>world.state130>>world.state140;
            for (unsigned i=0;i<3;++i) std::cin>>world.random.state[i];for (auto& v:s.waypoint) std::cin>>v;for (auto& v:s.direction) std::cin>>v;
            std::cin>>toggle>>latch>>s.threshold;for (auto& v:origin) std::cin>>v;for (auto& v:target) std::cin>>v;
            Check(bool(std::cin),"complete original replay input");s.toggle=std::uint8_t(toggle);s.latch=std::uint8_t(latch);if (speedKnown) s.speed=speed;
            world.Geometry(Floats(origin),Floats(target));auto time=world.timer.GetStateForAnalysis();time.currentMilliseconds=now;time.deltaSeconds=std::bit_cast<float>(dt);world.timer.SetStateForAnalysis(time);
            unsigned managerMode=2,targets=op=="e";std::uint32_t code=0x2717;
            if (op=="e0") std::cin>>managerMode;
            if (op=="q") std::cin>>targets;
            if (op=="o") std::cin>>code;
            Check(bool(std::cin),"complete boundary replay parameters");
            world.Manager(managerMode);world.Nearest(targets!=0);s.cachedNode=world.targetNode.get();unsigned returned=0;
            if (op=="u") returned=action.vfunc_24();
            else if (op=="e") { Check(action.EnterForAnalysis()==Result::Completed,"warm Entry");Check(action.QueryForAnalysis()==Result::Completed,"real nearest target Query"); }
            else if (op=="e0") Check(action.EnterForAnalysis()==Result::SelectorRequested,"known null target entry boundary");
            else if (op=="q") { const auto r=action.QueryForAnalysis();Check(r==Result::Completed || r==Result::SelectorRequested,"actual nearest query boundary"); }
            else if (op=="x") action.vfunc_34_ClearForAnalysis();
            else if (op=="n" || op=="o") { const std::array<std::uint32_t,8> frame{code,0,0,0,0,0,0x12345678,0};Check(action.NotifyForAnalysis(frame.data())==Result::Completed,"qualified own/common Notify"); }
            else if (op=="h") { const auto r=action.CheckHeightForAnalysis();Check(r==Result::Completed || r==Result::SelectorRequested,"strict-inside height boundary"); }
            else throw std::runtime_error("unknown original replay operation");
            Snapshot(action,world,returned);
        }
    }
    void Guards()
    {
        World w;wxIceGargoyleAttackAIAction guard(w);guard.SetOwnerForAnalysis(&w.owner);
        Check(!guard.StateForAnalysis().speed,"constructor speed is explicitly unknown");Check(!guard.vfunc_24(),"default active Update guard");
        Check(guard.EnterForAnalysis()==Result::UnqualifiedColdTargetManager,"cold manager guard");
        w.Manager();w.recordKnown=false;Check(guard.EnterForAnalysis()==Result::UnqualifiedTargetRecordData,"unknown record differs from known null");
        w.recordKnown=true;wxIceGargoyleAttackAIAction active(w,Profile::PcFiniteGeometry);active.SetOwnerForAnalysis(&w.owner);
        active.StateForAnalysis().phase=3;Check(active.UpdateForAnalysis()==Result::UnqualifiedSpeed,"uninitialized speed never guessed as zero");
        w.brainPresent=true;const std::array<std::uint32_t,8> frame{0x2717,0,0,0,0,0,7,0};active.StateForAnalysis().phase=0;
        Check(active.NotifyForAnalysis(frame.data())==Result::UnqualifiedBrainDelivery,"foreign brain Notify is not successful no-op");
        Check(active.IsExactly(wxIceGargoyleAttackAIAction::ClassID) && active.IsKindOf(wxAIAction::ClassID),"factory/physical kind relation");
        {
            World zero;zero.Manager();wxIceGargoyleAttackAIAction falling(zero,Profile::PcFiniteGeometry);falling.SetOwnerForAnalysis(&zero.owner);
            falling.StateForAnalysis().phase=3;falling.StateForAnalysis().speed=0x80000000;
            Check(falling.UpdateForAnalysis()==Result::Completed,"qualified zero-delta fall");
            Check(falling.StateForAnalysis().speed==std::optional<std::uint32_t>{0x80000000},"original -0 minus positive zero retains speed sign");
        }
    }
}
int main(int argc,char** argv)
{
    try { if (argc==2 && std::string(argv[1])=="--replay") Replay();else { Guards();std::cout<<"GargoyleAttack bounded contracts PASS\n"; }return 0; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
