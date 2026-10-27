#include "Code/wxCharacterMoveCtrl.h"
#include "Code/wxMinotaurAttackAIAction.h"
#include "Code/wxPerception.h"
#include "Code/wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxCharacterMoveCtrlAbi.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
    struct DataHost final:wxEntityHost
    {
        unsigned adds=0,moves=0,destroys=0;
        void AddToEngineManagerForAnalysis(spEntity&) override { ++adds; }
        void MoveFromEngineToGameForAnalysis(wxEntity&) override { ++moves; }
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override { ++destroys; }
        void DestroyEntityForAnalysis(spEntity&) noexcept override {}
        std::uint16_t GetReferenceCountForAnalysis(void*) override { std::terminate(); }
        void SetReferenceCountForAnalysis(void*,std::uint16_t) override { std::terminate(); }
        void DeleteReferenceForAnalysis(void*) override { std::terminate(); }
        void CompleteCopyForAnalysis(const wxEntity&,wxEntity&,spCloneManager&,std::uint32_t) const override { std::terminate(); }
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override { std::terminate(); }
        bool CachedPredicateForAnalysis(const wxEntity&,std::size_t) const override { std::terminate(); }
        bool TimerByteForAnalysis(const wxEntity&) const override { std::terminate(); }
        float SquaredDistanceForAnalysis(const wxEntity&) const override { std::terminate(); }
        void WriteComputedFlagsForAnalysis(wxEntity&,std::uint32_t) override { std::terminate(); }
    };
    unsigned Nibble(char value)
    { if (value>='0'&&value<='9') return unsigned(value-'0');if (value>='a'&&value<='f') return unsigned(value-'a'+10);throw std::invalid_argument("lowercase hex"); }
    wxCharacterMoveCtrl::CapturedPostStateForAnalysis Captured(const std::string& hex)
    {
        std::array<std::uint8_t,0x1C18> bytes{};Check(hex.size()==bytes.size()*2,"complete declared post-state input");
        for (std::size_t i=0;i<bytes.size();++i) bytes[i]=std::uint8_t((Nibble(hex[2*i])<<4)|Nibble(hex[2*i+1]));
        wxCharacterMoveCtrl::CapturedPostStateForAnalysis state;
        std::memcpy(&state.entityField20,bytes.data()+0x20,4);
        std::memcpy(state.entityFields.data(),bytes.data()+0x28,state.entityFields.size());
        std::memcpy(state.controllerFields.data(),bytes.data()+0x124,state.controllerFields.size());return state;
    }
    template<class Bytes>void Hex(std::ostream& out,const Bytes& bytes)
    { constexpr char hex[]="0123456789abcdef";for (const auto b:bytes) out<<hex[b>>4]<<hex[b&15]; }
    void Dump(const wxCharacterMoveCtrl& controller)
    {
        Hex(std::cout,controller.GetControllerBytesForAnalysis());std::cout<<' ';
        Hex(std::cout,controller.wxEntity::GetOwnBytesForAnalysis());std::cout<<' '<<controller.GetField20ForAnalysis();
    }
    struct World final:wxMinotaurAttackAIActionHost,wxPerceptionHost
    {
        int owner=1,target=2;spNode ownerNode,targetNode;
        std::array<std::uint8_t,0x74> command{};
        spTimer clock;wxGameTimer timer{clock};wxPerception perception;
        spFunctionEval::RandomStateForAnalysis random;
        wxCharacterMoveCtrl* controller=nullptr;
        explicit World(unsigned axis)
        {
            command.fill(0xA5);targetNode.SetPositionForAnalysis(axis?spNode::Vector3{0,500,0}:spNode::Vector3{500,0,0});
            targetNode.MarkLocalTransformDirtyForAnalysis();Check(targetNode.UpdateWorldForAnalysis(),"actual shared target WORLD cache");
        }
        ~World() override { perception.ResetForAnalysis(); }
        wxPerception& ReadPerceptionForAnalysis(void*) noexcept override { std::terminate(); }
        wxGameTimer& GameTimerForAnalysis() noexcept override { std::terminate(); }
        spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept override { std::terminate(); }
        void* ReadCommandForAnalysis(void*) noexcept override { std::terminate(); }
        std::uint8_t ReadCommandByteForAnalysis(void*,std::uint32_t) noexcept override { std::terminate(); }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        { if (value!=command.data()||offset>=command.size()) std::terminate();command[offset]=byte; }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        { if (value!=command.data()||offset+4>command.size()) std::terminate();std::memcpy(command.data()+offset,&word,4); }
        spNode& ReadNodeForAnalysis(void* value) noexcept override
        { if (value==&owner) return ownerNode;if (value==&target) return targetNode;std::terminate(); }
        std::uint32_t ReadCharacterState130ForAnalysis(void*) noexcept override { std::terminate(); }
        wxCharacterMoveCtrl* ReadMoveControllerForAnalysis(void* value) noexcept override
        { if (value!=&owner) std::terminate();return controller; }
        void SelectActionForAnalysis(void*,std::uint32_t,std::uint32_t) noexcept override { std::terminate(); }
        std::array<float,3> ReadNodePositionForAnalysis(const void*) const override { std::terminate(); }
        void DetachOwnerForAnalysis(wxPerception&,void*) noexcept override {}
    };
    void Replay()
    {
        std::string operation,hex;
        while (std::cin>>operation>>hex)
        {
            unsigned argument;std::cin>>argument;Check(bool(std::cin),"complete controller replay input");
            DataHost host;const auto input=Captured(hex);wxCharacterMoveCtrl controller(host,input);
            Check(host.adds==1&&host.moves==1,"known wxEntity base constructor profile used by data adapter");
            if (operation=="r")
            { Check(argument==1||argument==2,"reset repeat bound");for(unsigned i=0;i<argument;++i)controller.ResetFieldsForAnalysis();Dump(controller);std::cout<<'\n'; }
            else if (operation=="m")
            {
                World world(argument);world.controller=&controller;
                wxMinotaurAttackAIAction action(world,wxMinotaurAttackProfileForAnalysis::PcMovementWithCapturedControllerReset);
                action.SetOwnerForAnalysis(&world.owner);auto& state=action.StateForAnalysis();
                state.target=&world.target;state.phase=2;state.deadline=0x12345678;state.command=world.command.data();state.byte3BC=0xA5;
                const auto beforeOwner=world.ownerNode.GetWorldPositionForAnalysis(),beforeTarget=world.targetNode.GetWorldPositionForAnalysis();
                Check(action.vfunc_24()&&action.LastResultForAnalysis()==wxMinotaurAttackResultForAnalysis::Completed,"whole controller-backed Minotaur Update");
                Dump(controller);std::cout<<' '<<state.phase<<' '<<state.deadline<<' '<<unsigned(state.byte3BC)<<' ';
                std::array<std::uint8_t,0x70> payload{};std::memcpy(payload.data(),world.command.data()+4,payload.size());Hex(std::cout,payload);
                std::cout<<' '<<unsigned(beforeOwner==world.ownerNode.GetWorldPositionForAnalysis()&&beforeTarget==world.targetNode.GetWorldPositionForAnalysis())<<'\n';
            }
            else throw std::invalid_argument("controller replay operation");
        }
    }
    void Contracts()
    {
        DataHost host;wxCharacterMoveCtrl::CapturedPostStateForAnalysis input{};input.entityField20=1;
        input.entityFields.fill(0x5A);input.controllerFields.fill(0xA5);wxCharacterMoveCtrl controller(host,input),destination(host,input);
        Check(controller.IsKindOf(wxEntity::ClassID)&&wxCharacterMoveCtrl::StaticRTTI().factory==nullptr,"known RTTI but unsupported full factory is not native abstractness");
        const auto before=destination.GetControllerBytesForAnalysis();
        const auto baseBefore=destination.wxEntity::GetOwnBytesForAnalysis();spCloneManager clones;
        Check(!controller.vfunc_10(clones)&&!controller.vfunc_14(destination,clones),"unsupported derived Clone/Copy do not inherit base-only success");
        Check(destination.GetControllerBytesForAnalysis()==before&&destination.wxEntity::GetOwnBytesForAnalysis()==baseBefore,"refusals precede effects");
        controller.ResetFieldsForAnalysis();
        Check(controller.GetControllerBytesForAnalysis()!=input.controllerFields&&controller.wxEntity::GetOwnBytesForAnalysis()==input.entityFields&&controller.GetField20ForAnalysis()==1,"reset changes own data while preserving represented base input; native replay verifies exact cells");
        World world(0);wxMinotaurAttackAIAction action(world,wxMinotaurAttackProfileForAnalysis::PcMovementWithCapturedControllerReset);action.SetOwnerForAnalysis(&world.owner);
        auto& state=action.StateForAnalysis();state.target=&world.target;state.phase=2;state.command=world.command.data();
        Check(!action.vfunc_24()&&action.LastResultForAnalysis()==wxMinotaurAttackResultForAnalysis::UnqualifiedControllerReset,"missing binding never becomes successful controller stub");
    }
}
int main(int argc,char** argv)
{
    try { if (argc==2&&std::string(argv[1])=="--replay") Replay();else { Contracts();std::cout<<"MoveCtrl bounded reset contracts passed\n"; }return 0; }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
