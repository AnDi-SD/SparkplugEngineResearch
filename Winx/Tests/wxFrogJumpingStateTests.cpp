#include "Code/wxFrogJumpingState.h"
#include "Analysis/Host/wxFrogJumpingStateHost.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value, const char* message)
    { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    float Float(std::uint32_t value) { float f; std::memcpy(&f,&value,4); return f; }
    std::uint32_t Bits(float f) { std::uint32_t value; std::memcpy(&value,&f,4); return value; }
    unsigned Flags(const wxCharacterState& state)
    {
        unsigned result=0; const auto flags=state.GetTransitionFlagsForAnalysis();
        for (unsigned i=0;i<5;++i) if (flags[i]) result|=1u<<i;
        return result;
    }
    constexpr const char* names[]={"event_jump_begin","event_jump_end","event_jump_begin_extra",
        "prefix_event_jump_end_suffix","Event_jump_begin","event_jump_begi","",
        "event_jump_end event_jump_begin","prefix_event_jump_begin_suffix","unrelated"};
    struct Host final : wxFrogJumpingStateHost
    {
        wxFrogJumpingState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0; std::uintptr_t first=0,second=0;
        unsigned eventIndex=0;
        wxFrogJumpControlForAnalysis control{{Float(0x3F800001),Float(0x40000001),Float(0x40400001)},false};
        std::string trace;
        Host(wxFrogJumpingState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'
                +std::to_string(state.GetField3CForAnalysis() ? unsigned(*state.GetField3CForAnalysis()) : 256u);
        }
        const char* EventTagNameForAnalysis(const void*) override
        { Require(eventIndex<10,"event index"); return names[eventIndex]; }
        wxFrogJumpControlForAnalysis& OwnerEntityJumpControlForAnalysis(void*) override
        { return control; }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override
        { Event("reset:"+std::to_string(Token(handle))); if(first==Token(handle)) first=0; if(second==Token(handle)) second=0; }
        void StartAnimationForAnalysis(void*,void* handle,bool mode,std::uint32_t fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*,void* handle) override { Event("stop:"+std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override
        { Require(duration==0.4f,"native fade"); Event("fade:"+std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {
            Require(consume,"consume completion"); const auto token=Token(handle); Event("query:"+std::to_string(token));
            if (!token) return true;
            const bool complete=first==token || second==token;
            if (first==token) first=0;
            if (second==token) second=0;
            return complete;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word"); word=0; }
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        bool predicate,std::uintptr_t first,std::uintptr_t second,unsigned flags,
        std::uint32_t motion,std::uint8_t own,unsigned eventIndex,std::uint32_t height)
    {
        wxFrogJumpingState state; wxAnimationRequestForAnalysis request{key}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        state.SetField3CForAnalysis(own); state.SetField40ForAnalysis(Float(height));
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second;
        host.word=motion; host.eventIndex=eventIndex;
        unsigned result=2;
        if (slot==0x1C) result=state.vfunc_1C(request);
        else if (slot==0x20) result=state.vfunc_20(request);
        else if (slot==0x24) state.vfunc_24();
        else if (slot==0x28) state.vfunc_28(request);
        else if (slot==0x2C) state.vfunc_2C(request);
        else if (slot==0x30) state.vfunc_30(request);
        else if (slot==0x34) result=state.vfunc_34(key);
        else if (slot==0x38) result=state.vfunc_38(key);
        else if (slot==0x3C) state.vfunc_3C(Pointer(0x300));
        else if (slot==0x40) state.vfunc_40_ResetForAnalysis();
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)
            <<' '<<unsigned(*state.GetField3CForAnalysis())<<' '<<Bits(state.GetField40ForAnalysis());
        for(float f:host.control.velocity) output<<' '<<Bits(f);
        output<<' '<<host.control.enabled<<'|'<<host.trace;
        return output.str();
    }
}
int main(int argc,char** argv)
{
    if (argc==14 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[12]{}; for(unsigned i=0;i<12;++i) v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,v[5],v[6],
            unsigned(v[7]),std::uint32_t(v[8]),std::uint8_t(v[9]),unsigned(v[10]),std::uint32_t(v[11]))<<'\n';
        return EXIT_SUCCESS;
    }
    wxFrogJumpingState state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
    Require(state.IsExactly(wxFrogJumpingState::ClassID) && state.IsKindOf(wxCharacterState::ClassID)
        && state.GetStateSelectorForAnalysis()==1 && !state.GetField3CForAnalysis()
        && state.GetField40ForAnalysis()==375,"native identity/defaults and unspecified byte");
    bool rejected=false; try { state.vfunc_30(request); } catch(const std::logic_error&) { rejected=true; }
    Require(rejected,"unspecified byte is not synthesized as zero");
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); state.SetPendingHandleForAnalysis(Pointer(22));
    host.next=Pointer(22); host.word=99; host.first=host.second=22;
    Require(state.vfunc_1C(request) && state.GetField3CForAnalysis()==0
        && request.packedKey==0xF01F805F && !host.first && !host.second && host.word==99
        && host.trace.find("start:22:0:0:1")!=std::string::npos
        && host.trace.find("fade:")==std::string::npos && host.trace.find("stop:")==std::string::npos,
        "entry always queues same handle and skips update/release");
    host.eventIndex=8; state.vfunc_3C(Pointer(3));
    Require(state.GetField3CForAnalysis()==1 && host.control.velocity==std::array<float,3>{0,375,0}
        && host.control.enabled,"embedded jump-begin writes entity jump control");
    state.vfunc_30(request); Require(host.word==99,"active jump leaves direct action control");
    host.eventIndex=3; state.vfunc_3C(Pointer(3)); state.vfunc_30(request);
    Require(state.GetField3CForAnalysis()==0 && !host.word && host.control.enabled
        && host.control.velocity[1]==375,"jump-end only clears byte; update clears separate direct control");
    host.eventIndex=7; state.vfunc_3C(Pointer(3)); Require(state.GetField3CForAnalysis()==1,"begin precedence over end");
    host.trace.clear(); Require(state.vfunc_34(10) && host.trace.empty(),"codeA bypasses completion");
    host.first=22; Require(state.vfunc_34(0) && !host.first && !state.vfunc_34(0),"completion consumes records");
    state.SetField3CForAnalysis(255); state.SetField40ForAnalysis(-1);
    auto clone=state.Clone(); auto* fresh=dynamic_cast<wxFrogJumpingState*>(clone.get());
    Require(fresh && !fresh->GetField3CForAnalysis() && fresh->GetField40ForAnalysis()==375
        && !fresh->GetPendingHandleForAnalysis(),"clone defaults do not copy own fields");
    wxFrogJumpingState target; target.SetField3CForAnalysis(7); target.SetField40ForAnalysis(12);
    spCloneManager manager; Require(state.vfunc_14(target,manager) && target.GetField3CForAnalysis()==7
        && target.GetField40ForAnalysis()==12,"empty copy preserves own target");
    state.vfunc_40_ResetForAnalysis(); Require(state.GetField3CForAnalysis()==255
        && state.GetField40ForAnalysis()==-1 && !state.GetPendingHandleForAnalysis(),"base reset preserves own fields");
    Require(dynamic_cast<wxFrogJumpingState*>(spRTTIManager::Instance().Create(wxFrogJumpingState::ClassID).get()),"factory");
    std::cout<<"Frog jumping state checks passed\n";
}
