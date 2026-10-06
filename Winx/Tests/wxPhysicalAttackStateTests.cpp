#include "Code/wxPhysicalAttackState.h"
#include "Analysis/Host/wxPhysicalAttackStateHost.h"
#include "Analysis/PC/wxPhysicalAttackStateAbi.h"
#include "Analysis/PS2/wxPhysicalAttackStateAbi.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v, const char* text) { if (!v) throw std::runtime_error(text); }
    void* Pointer(std::uintptr_t v) { return reinterpret_cast<void*>(v); }
    std::uintptr_t Token(void* v) { return reinterpret_cast<std::uintptr_t>(v); }
    unsigned Flags(const wxCharacterState& s)
    { unsigned v=0; const auto f=s.GetTransitionFlagsForAnalysis(); for(unsigned i=0;i<5;++i) if(f[i]) v|=1u<<i; return v; }
    const char* Tags[]={"event_impact_begin","event_impact_end","event_impact_begin_extra",
        "event_impact_end_extra","prefix_event_impact_begin","Event_impact_begin","","event_shoot"};
    struct Host final : wxPhysicalAttackStateHost
    {
        wxPhysicalAttackState& state; wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22, first=0, second=0;
        unsigned mutation=0, tag=0; bool predicate=false;
        std::uint32_t word=0x12345678; std::string trace;
        Host(wxPhysicalAttackState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text)
        { if(!trace.empty()) trace+=';'; trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey); }
        void Mutate() { state.SetBindingsForAnalysis(Pointer(0x101),Pointer(0x200),this); }
        const char* EventTagNameForAnalysis(const void*) override
        { Event("tag"); Check(tag<8,"tag index"); return Tags[tag]; }
        void* OwnerField124ForAnalysis(void* owner) override
        { Event("owner124:"+std::to_string(Token(owner))); return mutation&16 ? nullptr : Pointer(owner==Pointer(0x100)?0x400:0x401); }
        void* OwnerField24ForAnalysis(void*) override { return mutation&8 ? nullptr : Pointer(0x500); }
        void SendFilteredNotificationForAnalysis(wxCharacterState& source,std::uint32_t code,
            std::uint32_t filter,void* payload0,std::uint32_t payload1) override
        { Check(&source==&state&&code==0x2731&&filter==6&&payload1==0,"filtered packet"); Event("message:"+std::to_string(code)+':'+std::to_string(filter)+':'+std::to_string(Token(payload0))+':'+std::to_string(payload1)); }
        void SendImpactNotificationForAnalysis(void* receiver,wxCharacterState& source,
            const char* boneName,bool begin) override
        { Check(receiver==Pointer(0x500)&&&source==&state&&std::strcmp(boneName,"foot_left")==0,"typed packet known fields"); Event("impact:10015:foot_left:"+std::to_string(begin)); }
        void* ResolveAnimationForAnalysis(void* owner,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)+':'+std::to_string(Token(owner))); if(mutation&2) Mutate(); return Pointer(next); }
        bool OwnerPredicateForAnalysis(void* owner) override
        { Event("predicate:"+std::to_string(Token(owner))); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override
        { Event("reset:"+std::to_string(Token(handle))); if(first==Token(handle)) first=0; if(second==Token(handle)) second=0; }
        void StartAnimationForAnalysis(void*,void* handle,bool mode,std::uint32_t fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt)); if(mutation&4) Mutate(); }
        void StopAnimationForAnalysis(void*,void* handle) override { Event("stop:"+std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*,void* handle,float) override { Event("fade:"+std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {
            Check(consume,"consume completion"); Event("query:"+std::to_string(Token(handle)));
            if(!handle) return true;
            const auto v=Token(handle); const bool matched=first==v||second==v;
            if(first==v) first=0; if(second==v) second=0; return matched;
        }
        void ClearOwnerActionControlForAnalysis(void* owner) override
        { Check(owner==Pointer(0x100),"initial action control"); Event("word"); word=0; if(mutation&1) Mutate(); }
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned mutation,bool predicate)
    {
        wxPhysicalAttackState state; wxAnimationRequestForAnalysis request{key}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next; host.first=first; host.second=second; host.mutation=mutation; host.tag=unsigned(first); host.predicate=predicate;
        unsigned result=2;
        if(slot==0x1C) result=state.vfunc_1C(request);
        else if(slot==0x30) state.vfunc_30(request);
        else if(slot==0x34) result=state.vfunc_34(key);
        else if(slot==0x38) result=state.vfunc_38(key);
        else if(slot==0x3C) state.vfunc_3C(Pointer(0x300));
        else if(slot==0x40) state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("case slot");
        std::ostringstream out; out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '
            <<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace; return out.str();
    }
    void Lifecycle()
    {
        wxPhysicalAttackState source;
        Check(source.IsExactly(wxPhysicalAttackState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==19,"RTTI and selector");
        Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"defaults");
        Check(dynamic_cast<wxPhysicalAttackState*>(spRTTIManager::Instance().Create(wxPhysicalAttackState::ClassID).get()),"factory");
        source.SetPendingHandleForAnalysis(Pointer(11)); source.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=source.Clone(); auto* fresh=dynamic_cast<wxPhysicalAttackState*>(clone.get());
        Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone constructs fresh fields");
        wxPhysicalAttackState target; target.SetPendingHandleForAnalysis(Pointer(7)); spCloneManager manager;
        Check(source.vfunc_14(target,manager)&&Token(target.GetPendingHandleForAnalysis())==7,"Copy no-op");
        source.vfunc_40_ResetForAnalysis(); Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==7,"base reset");
        bool threw=false; try { source.vfunc_34(9); } catch(const std::logic_error&) { threw=true; }
        Check(threw,"null completion still requires real host");
    }
    void EntryAndEvents()
    {
        struct Derived final:wxPhysicalAttackState { unsigned updates=0; void vfunc_30(wxAnimationRequestForAnalysis&) override { ++updates; } };
        Derived state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        Check(state.vfunc_1C(request)&&!state.updates&&host.word==0,"entry clear is direct before queue");
        Check(host.trace.find("word@0:31:4294967295")==0,"clear before mask");
        Check(host.trace.find("message:10033:6:1024:0@22")!=std::string::npos,"message after pending store");
        state.vfunc_3C(Pointer(0x300)); host.tag=1; state.vfunc_3C(Pointer(0x300));
        Check(host.trace.find("impact:10015:foot_left:1")!=std::string::npos&&host.trace.find("impact:10015:foot_left:0")!=std::string::npos,"begin/end typed delivery");
        Check(Case(0x3C,0,11,22,2,0,31,0,false).find("impact:")==std::string::npos,"full event equality");
        Check(Case(0x3C,0,11,22,0,0,31,8,false).find("impact:")==std::string::npos,"null receiver skips delivery");
        Check(Case(0x34,9,0,22,11,22,31,0,false).find("query:0")!=std::string::npos,"permission queries null");
        Check(state.vfunc_38(9)&&!state.vfunc_38(10),"hook38 code9 only");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==11&&std::string(argv[1])=="--case")
        { std::uint64_t v[9]{}; for(unsigned i=0;i<9;++i) v[i]=std::stoull(argv[i+2]); std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),v[8]!=0)<<'\n'; return 0; }
        Lifecycle(); EntryAndEvents(); std::cout<<"PASS PhysicalAttack lifecycle, entry, completion and typed impact delivery\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
