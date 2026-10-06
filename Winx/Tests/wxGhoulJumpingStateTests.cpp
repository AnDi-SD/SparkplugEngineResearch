#include "Code/wxGhoulJumpingState.h"
#include "Analysis/Host/wxGhoulJumpingStateHost.h"
#include "Analysis/PC/wxGhoulJumpingStateAbi.h"
#include "Analysis/PS2/wxGhoulJumpingStateAbi.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v,const char* text) { if(!v) throw std::runtime_error(text); }
    void* Pointer(std::uintptr_t v) { return reinterpret_cast<void*>(v); }
    std::uintptr_t Token(void* v) { return reinterpret_cast<std::uintptr_t>(v); }
    unsigned Flags(const wxCharacterState& s) { unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v; }
    float Float(std::uint32_t v) { float f;std::memcpy(&f,&v,4);return f; }
    std::uint32_t Bits(float f) { std::uint32_t v;std::memcpy(&v,&f,4);return v; }
    const char* const Tags[]={"air","airborne","xair","Air","","event_jump_begin","air_end"};
    struct Host final:wxGhoulJumpingStateHost
    {
        wxGhoulJumpingState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned mutation=0;bool predicate=false;
        std::uint32_t branch=0;unsigned tag=0;std::string trace;
        wxGhoulJumpActionControlForAnalysis action;
        wxFrogJumpControlForAnalysis jump{{1,2,3},false};
        Host(wxGhoulJumpingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text) { if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey); }
        void Mutate() { state.SetBindingsForAnalysis(Pointer(0x101),Pointer(0x200),this); }
        wxGhoulJumpActionControlForAnalysis& OwnerDirectActionControlForAnalysis(void*) override { return action; }
        const char* EventTagNameForAnalysis(const void*) override { return Tags[tag]; }
        wxFrogJumpControlForAnalysis& OwnerEntityJumpControlForAnalysis(void*) override { return jump; }
        std::uint32_t OwnerField218ForAnalysis(void*) override { return branch; }
        void CallOwnerField218BranchForAnalysis(void* owner,bool nonzero) override { Event("owner-branch:"+std::to_string(Token(owner))+':'+std::to_string(nonzero)); }
        void* ResolveAnimationForAnalysis(void* owner,std::uint32_t key) override { Event("lookup:"+std::to_string(key)+':'+std::to_string(Token(owner)));if(mutation&2)Mutate();return Pointer(next); }
        bool OwnerPredicateForAnalysis(void* owner) override { Event("predicate:"+std::to_string(Token(owner)));return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override { Event("reset:"+std::to_string(Token(handle)));if(first==Token(handle))first=0;if(second==Token(handle))second=0; }
        void StartAnimationForAnalysis(void*,void* handle,bool mode,std::uint32_t fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation&4)Mutate(); }
        void StopAnimationForAnalysis(void*,void* handle) override { Event("stop:"+std::to_string(Token(handle)));if(mutation&64)Mutate(); }
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override { Check(duration==.4f,"fade duration");Event("fade:"+std::to_string(Token(handle)));if(mutation&64)Mutate(); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        { Check(consume,"consume");Event("query:"+std::to_string(Token(handle)));if(!handle)return true;const auto v=Token(handle);const bool matched=first==v||second==v;if(first==v)first=0;if(second==v)second=0;return matched; }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word");action.field4=0; }
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned mutation,bool predicate,std::uint32_t branch,std::uint32_t motion,unsigned flag,unsigned tag)
    {
        wxGhoulJumpingState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.mutation=mutation;host.predicate=predicate;host.branch=branch;host.action={Float(motion),std::uint8_t(flag)};Check(tag<7,"tag");host.tag=tag;unsigned result=4;
        if(slot==0x1C)result=state.vfunc_1C(request);else if(slot==0x20)result=state.vfunc_20(request);else if(slot==0x2C)state.vfunc_2C(request);else if(slot==0x30)state.vfunc_30(request);else if(slot==0x34)result=state.vfunc_34(key);else if(slot==0x38)result=state.vfunc_38(key);else if(slot==0x3C)state.vfunc_3C(nullptr);else if(slot==0x40)state.vfunc_40_ResetForAnalysis();else throw std::runtime_error("slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Bits(host.action.field4)<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<Bits(host.jump.velocity[0])<<' '<<Bits(host.jump.velocity[1])<<' '<<Bits(host.jump.velocity[2])<<' '<<host.jump.enabled<<'|'<<host.trace;return out.str();
    }
    void Lifecycle()
    {
        wxGhoulJumpingState source;Check(source.IsExactly(wxGhoulJumpingState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==1,"RTTI/selector");Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"defaults");
        Check(dynamic_cast<wxGhoulJumpingState*>(spRTTIManager::Instance().Create(wxGhoulJumpingState::ClassID).get()),"factory");source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);auto clone=source.Clone();auto* fresh=dynamic_cast<wxGhoulJumpingState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"fresh clone");
        wxGhoulJumpingState target;target.SetPendingHandleForAnalysis(Pointer(7));spCloneManager manager;Check(source.vfunc_14(target,manager)&&Token(target.GetPendingHandleForAnalysis())==7,"Copy no-op");source.vfunc_40_ResetForAnalysis();Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==7,"base reset");
        Check(!source.vfunc_34(0x17),"permission17 requires no host");bool threw=false;try{(void)source.vfunc_34(9);}catch(const std::logic_error&){threw=true;}Check(threw,"other permission requires host even null");
        const wxCharacterState& polymorphic=source;Check(polymorphic.vfunc_38(0)==2&&polymorphic.vfunc_38(3)==2&&polymorphic.vfunc_38(9)==1,"raw integer dispatch");
    }
    void CompleteCycle()
    {
        wxGhoulJumpingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);host.action={.2f,0};host.predicate=true;
        Check(state.vfunc_1C(request)&&request.packedKey==0xF0BF805F&&Token(state.GetPendingHandleForAnalysis())==22,"entry equal threshold high mode");
        Check(host.trace.find("start:22:0:2:1@0")!=std::string::npos&&Bits(host.action.field4)==0x3E4CCCCD,"queue before pending without action clear");
        state.vfunc_3C(nullptr);Check(host.jump.velocity==std::array<float,3>{0,375,0}&&host.jump.enabled,"air jump control");host.trace.clear();host.first=22;Check(!state.vfunc_34(0x17)&&host.first==22&&host.trace.empty(),"17 preserves completion");Check(state.vfunc_34(9)&&host.first==0,"other consumes completion");
        host.trace.clear();host.branch=0xFFFFFFFF;state.vfunc_2C(request);Check(!state.GetPendingHandleForAnalysis()&&host.trace.find("owner-branch:256:1@0")!=std::string::npos,"release before unsigned branch");
        Check(Case(0x2C,0,11,22,0,0,31,64,false,0,0,0,0).find("owner-branch:257:0@0")!=std::string::npos,"owner reload after release");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==15&&std::string(argv[1])=="--case") { std::uint64_t v[13]{};for(unsigned i=0;i<13;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),v[8]!=0,std::uint32_t(v[9]),std::uint32_t(v[10]),unsigned(v[11]),unsigned(v[12]))<<'\n';return 0; }
        Lifecycle();CompleteCycle();std::cout<<"PASS GhoulJumping lifecycle, entry/event/permission/release cycle and integer dispatch\n";return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
