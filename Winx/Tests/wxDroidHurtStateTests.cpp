#include "Code/wxDroidHurtState.h"
#include "Analysis/Host/wxDroidHurtStateHost.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
    void* Pointer(std::uintptr_t x) { return reinterpret_cast<void*>(x); }
    std::uintptr_t Token(void* x) { return reinterpret_cast<std::uintptr_t>(x); }
    std::uint32_t Bits(float x) { std::uint32_t n; std::memcpy(&n, &x, 4); return n; }
    struct Host final : wxDroidHurtStateHost
    {
        wxDroidHurtState& state; wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22, first=0, second=0; bool predicate=false, receiver=true, node=true;
        unsigned raw=0; std::uint32_t clock=100, control=0xffffffff, nodeDirty=0x10, targetDirty=0x20;
        Vector3 position{2,-3,4}, target{10,20,30}; bool ps2=false; std::string trace;
        Host(wxDroidHurtState& s, wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        unsigned Flags() const
        { unsigned n=0; auto flags=state.GetTransitionFlagsForAnalysis(); for(unsigned i=0;i<5;++i) if(flags[i])n|=1u<<i; return n; }
        void Event(const std::string& name)
        { if(!trace.empty())trace+=';';trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags())+':'+std::to_string(request.packedKey); }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override { Event("lookup:"+std::to_string(key)); return Pointer(next); }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* h) override { Event("reset:"+std::to_string(Token(h))); }
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*,void* h) override { Event("stop:"+std::to_string(Token(h))); }
        void FadeAnimationForAnalysis(void*,void* h,float duration) override { Check(duration==0.4f,"fade"); Event("fade:"+std::to_string(Token(h))); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {
            Check(consume,"consuming query");const auto token=Token(h);Event("query:"+std::to_string(token));
            if(!token)return true;const bool done=first==token||second==token;
            if(first==token)first=0;if(second==token)second=0;return done;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word");control=0; }
        std::uint8_t ReadHurtControlByteForAnalysis(void*) override { Event("read-control");return std::uint8_t(raw); }
        void ClearHurtControlByteForAnalysis(void*) override { Event("clear-control");raw=0; }
        std::uint32_t ClockWordForAnalysis() override { Event("clock");return clock; }
        bool HasHurtMessageReceiverForAnalysis(void*) override { return receiver; }
        void SendHurtMessageForAnalysis(void*,const MessageForAnalysis& m) override
        {
            Check(m.code==0x27d1 && m.words04==std::array<std::uint32_t,3>{0,0,0}
                && m.source==&state && m.word14==0 && m.word18==0x6c && m.word1C==0,"message exact words");
            Event("message");
        }
        void* FindMovementNodeForAnalysis(void*,const char* name,bool recursive,bool last) override
        { Check(std::strcmp(name,"movement_tracker")==0 && recursive && !last,"movement lookup"); Event("find-node");return node?Pointer(1):nullptr; }
        float ReadNodePositionWordForAnalysis(void* n,std::uint32_t offset) override
        { return (n==Pointer(1)?position:target).at((offset-0x20)/4); }
        void WriteNodePositionWordForAnalysis(void* n,std::uint32_t offset,float value) override
        { Event(std::string(n==Pointer(1)?"node:":"target:")+std::to_string(offset)+':'+std::to_string(Bits(value)));(n==Pointer(1)?position:target).at((offset-0x20)/4)=value; }
        void MarkMovementNodeDirtyForAnalysis(void* n) override
        { Event(n==Pointer(1)?"node-dirty":"target-dirty");(n==Pointer(1)?nodeDirty:targetDirty)|=1; }
        void* OwnerMovementTargetForAnalysis(void*) override { return Pointer(2); }
        Matrix3 ReadMovementTargetOrientationForAnalysis(void*) override { return {1,0,0,0,1,0,0,0,1}; }
        wxCharacterMovementNumericProfileForAnalysis MovementNumericProfileForAnalysis() const noexcept override
        { return ps2?wxCharacterMovementNumericProfileForAnalysis::PS2Finite:wxCharacterMovementNumericProfileForAnalysis::PCFinite; }
    };
    std::string Case(const std::uint64_t* v)
    {
        wxDroidHurtState state; wxAnimationRequestForAnalysis request{std::uint32_t(v[1])}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(100),Pointer(200),&host);state.SetPendingHandleForAnalysis(Pointer(v[2]));
        state.SetTransitionFlagsForAnalysis(v[5]&1,v[5]&2,v[5]&4,v[5]&8,v[5]&16);
        state.SetRuntimeForAnalysis({73,91});host.next=v[3];host.predicate=v[4]!=0;host.first=v[6];host.second=v[7];
        host.raw=unsigned(v[8]);host.clock=std::uint32_t(v[9]);host.receiver=v[10]!=0;host.node=v[11]!=0;
        state.SetMovementFieldsForAnalysis(host.node?Pointer(1):nullptr,v[12]!=0,{-1,2,-3});unsigned result=2;
        switch(v[0])
        {
        case 0x1c:result=state.vfunc_1C(request);break;case 0x20:result=state.vfunc_20(request);break;
        case 0x24:state.vfunc_24();break;case 0x28:state.vfunc_28(request);break;case 0x2c:state.vfunc_2C(request);break;
        case 0x30:state.vfunc_30(request);break;case 0x34:result=state.vfunc_34(request.packedKey);break;
        case 0x38:result=state.vfunc_38(request.packedKey);break;case 0x3c:state.vfunc_3C(nullptr);break;
        case 0x40:state.vfunc_40_ResetForAnalysis();break;default:throw std::logic_error("slot");
        }
        auto runtime=state.GetRuntimeForAnalysis();auto movement=state.GetMovementFieldsForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.control
            <<' '<<host.first<<' '<<host.second<<' '<<host.Flags()<<' '<<host.raw<<' '<<runtime.deadline3C<<' '<<runtime.word40
            <<' '<<Token(movement.field28)<<' '<<movement.field2C;
        for(auto x:movement.resetValues)out<<' '<<Bits(x);for(auto x:host.position)out<<' '<<Bits(x);for(auto x:host.target)out<<' '<<Bits(x);
        out<<' '<<host.nodeDirty<<' '<<host.targetDirty<<'|'<<host.trace;return out.str();
    }
    void Lifetime()
    {
        wxDroidHurtState state; Check(state.GetStateSelectorForAnalysis()==10,"selector");
        state.SetRuntimeForAnalysis({9,8});state.SetPendingHandleForAnalysis(Pointer(11));state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        spCloneManager manager;auto clone=state.vfunc_10(manager);auto* fresh=dynamic_cast<wxDroidHurtState*>(clone.get());
        Check(fresh && manager.FindClone(state)==fresh && fresh->GetRuntimeForAnalysis().deadline3C==0
            && fresh->GetRuntimeForAnalysis().word40==0 && !fresh->GetPendingHandleForAnalysis()
            && fresh->GetTransitionFlagsForAnalysis()==std::array<bool,5>{true,true,true,true,true},"fresh clone runtime");
        Check(fresh->IsExactly(state.ClassID) && fresh->IsKindOf(wxCharacterState::ClassID)
            && bool(spRTTIManager::Instance().Create(state.ClassID)),"RTTI/factory");
        state.vfunc_40_ResetForAnalysis();Check(state.GetRuntimeForAnalysis().deadline3C==9 && state.GetRuntimeForAnalysis().word40==8,"base reset retains derived fields");
        wxAnimationRequestForAnalysis request;bool rejected=false;try{state.vfunc_1C(request);}catch(const std::logic_error&){rejected=true;}Check(rejected,"missing host");
        Host host(state,request);host.ps2=true;host.raw=255;state.SetBindingsForAnalysis(nullptr,nullptr,&host);state.vfunc_1C(request);
        Check(request.packedKey==0x0f808000,"PS2 five-bit variant");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==15 && std::string(argv[1])=="--case")
        { std::uint64_t values[13];for(unsigned i=0;i<13;++i)values[i]=std::stoull(argv[i+2]);std::cout<<Case(values)<<'\n';return 0; }
        Lifetime();std::uint64_t v[13]{0x1c,0xffffffff,11,22,0,31,11,11,255,0xfffffff0,1,1,0};
        Check(Case(v).find("stop")==std::string::npos,"entry retains old pending without release");
        v[0]=0x30;Check(Case(v).find("find-node")!=std::string::npos,"movement lookup and update");
        std::cout<<"PASS DroidHurt lifecycle and operations\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
