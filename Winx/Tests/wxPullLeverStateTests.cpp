#include "Code/wxPullLeverState.h"
#include "Analysis/Host/wxPullLeverStateHost.h"
#include "Analysis/PC/wxPullLeverStateAbi.h"
#include "Analysis/PS2/wxPullLeverStateAbi.h"
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
    struct Host final:wxPullLeverStateHost
    {
        wxPullLeverState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned mutation=0;bool predicate=false;
        std::uint32_t word=0x12345678,branch=0;bool controllerFlag=false;std::string trace;
        Host(wxPullLeverState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text) { if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey); }
        void Mutate() { state.SetBindingsForAnalysis(Pointer(0x101),Pointer(0x200),this); }
        const char* EventTagNameForAnalysis(const void*) override { throw std::logic_error("PullLever event adapter is not invoked"); }
        void* OwnerField124ForAnalysis(void* owner) override { Event("owner124:"+std::to_string(Token(owner)));return Pointer(owner==Pointer(0x100)?0x400:0x401); }
        void SendFilteredNotificationForAnalysis(wxCharacterState& source,std::uint32_t code,std::uint32_t filter,void* entity,std::uint32_t payload1) override
        { Check(&source==&state&&code==0x272E&&filter==6&&payload1==0,"filtered packet");Event("message:"+std::to_string(code)+':'+std::to_string(filter)+':'+std::to_string(Token(entity))+":0");if(mutation&32)Mutate(); }
        void SetOwnerEntityControllerFlagForAnalysis(void* owner,bool value) override { Event("entity-flag:"+std::to_string(Token(owner))+':'+std::to_string(value));controllerFlag=value; }
        void* GlobalPlayerForAnalysis() override { Event("global-player");return Pointer(0x600); }
        void ResetPlayerControllerForAnalysis(void* player) override { Check(player==Pointer(0x600),"borrowed player");Event("player-reset:"+std::to_string(Token(player))); }
        void* GlobalField2B4ReceiverForAnalysis() override { return mutation&8?nullptr:Pointer(0x500); }
        void SendLeverFlagNotificationForAnalysis(void* receiver,wxCharacterState& source,bool value) override
        { Check(receiver==Pointer(0x500)&&&source==&state,"typed receiver");Event("lever:10039:"+std::to_string(value)); }
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
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word");word=0;if(mutation&1)Mutate(); }
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned mutation,bool predicate,std::uint32_t branch)
    {
        wxPullLeverState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.mutation=mutation;host.predicate=predicate;host.branch=branch;unsigned result=2;
        if(slot==0x1C)result=state.vfunc_1C(request);else if(slot==0x20)result=state.vfunc_20(request);else if(slot==0x2C)state.vfunc_2C(request);else if(slot==0x30)state.vfunc_30(request);else if(slot==0x34)result=state.vfunc_34(key);else if(slot==0x38)result=state.vfunc_38(key);else if(slot==0x40)state.vfunc_40_ResetForAnalysis();else throw std::runtime_error("slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<host.controllerFlag<<'|'<<host.trace;return out.str();
    }
    void Lifecycle()
    {
        wxPullLeverState source;Check(source.IsExactly(wxPullLeverState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==31,"RTTI/selector");Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"defaults");
        Check(dynamic_cast<wxPullLeverState*>(spRTTIManager::Instance().Create(wxPullLeverState::ClassID).get()),"factory");source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);auto clone=source.Clone();auto* fresh=dynamic_cast<wxPullLeverState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"fresh clone");
        wxPullLeverState target;target.SetPendingHandleForAnalysis(Pointer(7));spCloneManager manager;Check(source.vfunc_14(target,manager)&&Token(target.GetPendingHandleForAnalysis())==7,"Copy no-op");source.vfunc_40_ResetForAnalysis();Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==7,"base reset");
        bool threw=false;try{(void)source.vfunc_34(9);}catch(const std::logic_error&){threw=true;}Check(threw,"null query requires host");
    }
    void CompleteCycle()
    {
        wxPullLeverState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        Check(state.vfunc_1C(request)&&host.word==0&&host.controllerFlag,"entry activates controller");
        Check(host.trace.find("word@22:")!=std::string::npos&&host.trace.find("lever:10039:1@22")!=std::string::npos,"pending before clear and active notification");host.trace.clear();
        Check(state.vfunc_20(request)&&!host.controllerFlag&&Token(state.GetPendingHandleForAnalysis())==22,"exit preserves pending");Check(host.trace.find("stop:")==std::string::npos&&host.trace.find("lever:10039:0")!=std::string::npos,"exit only flag and notification");host.trace.clear();host.branch=0xFFFFFFFF;state.vfunc_2C(request);Check(!state.GetPendingHandleForAnalysis()&&host.trace.find("owner-branch:256:1@0")!=std::string::npos,"release before unsigned branch");
        Check(Case(0x20,0,11,22,0,0,31,8,false,0).find("lever:")==std::string::npos,"null global receiver");
        Check(Case(0x2C,0,11,22,0,0,31,64,false,0).find("owner-branch:257:0@0")!=std::string::npos,"owner reload after release callback");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==12&&std::string(argv[1])=="--case") { std::uint64_t v[10]{};for(unsigned i=0;i<10;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),v[8]!=0,std::uint32_t(v[9]))<<'\n';return 0; }
        Lifecycle();CompleteCycle();std::cout<<"PASS PullLever lifecycle, full entry/exit and release cycle\n";return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
