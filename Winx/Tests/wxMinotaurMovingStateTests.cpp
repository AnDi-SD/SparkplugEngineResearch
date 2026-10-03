#include "Code/wxMinotaurMovingState.h"
#include "Analysis/Host/wxMinotaurMovingStateHost.h"
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
    unsigned Flags(const wxCharacterState& state)
    {
        unsigned result=0; const auto flags=state.GetTransitionFlagsForAnalysis();
        for (unsigned i=0;i<5;++i) if (flags[i]) result|=1u<<i;
        return result;
    }
    struct Host final : wxMinotaurMovingStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0; std::uintptr_t first=0,second=0;
        std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+std::to_string(static_cast<wxMinotaurMovingState&>(state).GetField3CForAnalysis());
        }
        bool ps2=false,changeSecond=false;unsigned reads=0;std::uint32_t secondMotion=0;
        bool UsePS2FiniteMotionProfileForAnalysis() const noexcept override { return ps2; }
        void* OwnerControlObjectForAnalysis(void*) override { Event("control");reads=0;return Pointer(0x500); }
        float ReadControlMotionForAnalysis(void* control) override
        {
            Require(control==Pointer(0x500),"captured control");
            if(reads++ && changeSecond) word=secondMotion;
            Event("motion:"+std::to_string(word));float value;std::memcpy(&value,&word,4);return value;
        }
        void WriteControlWordForAnalysis(void* control,unsigned bits) override
        { Require(control==Pointer(0x500),"medium write uses captured control");Event("write:"+std::to_string(bits));word=bits; }
        void WriteOwnerActionControlForAnalysis(void*,unsigned bits) override
        { Event("write:"+std::to_string(bits));word=bits; }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override { Event("reset:"+std::to_string(Token(handle)));if(first==Token(handle)) first=0;if(second==Token(handle)) second=0; }
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
    std::string Case(unsigned profile,unsigned slot,std::uint32_t key,std::uintptr_t pending,
        std::uintptr_t next,bool predicate,std::uintptr_t first,std::uintptr_t second,unsigned flags,
        std::uint32_t motion,std::uint8_t lock,std::uint32_t secondMotion,bool changeSecond)
    {
        wxMinotaurMovingState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);state.SetField3CForAnalysis(lock);
        host.ps2=profile==1;host.next=Pointer(next);host.predicate=predicate;host.first=first;host.second=second;host.word=motion;host.secondMotion=secondMotion;host.changeSecond=changeSecond;
        unsigned result=2;
        if(slot==0x1C) result=state.vfunc_1C(request);
        else if(slot==0x20) result=state.vfunc_20(request);
        else if(slot==0x30) state.vfunc_30(request);
        else if(slot==0x34) result=state.vfunc_34(key);
        else throw std::logic_error("unknown minotaur slot");
        std::ostringstream output;output<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<unsigned(state.GetField3CForAnalysis())<<'|'<<host.trace;return output.str();
    }
    template<class State> void CheckLifetime(unsigned selector)
    {
        State state; Require(state.GetStateSelectorForAnalysis()==selector && state.IsExactly(State::ClassID)
            && state.IsKindOf(wxCharacterState::ClassID),"selector and RTTI");
        Require(dynamic_cast<State*>(spRTTIManager::Instance().Create(State::ClassID).get()),"factory");
        state.SetPendingHandleForAnalysis(Pointer(11)); state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone(); auto* fresh=dynamic_cast<State*>(clone.get());
        Require(fresh && fresh->GetStateSelectorForAnalysis()==selector && !fresh->GetPendingHandleForAnalysis()
            && Flags(*fresh)==31,"clone has fresh flags and pending");
        State target; spCloneManager manager; target.SetPendingHandleForAnalysis(Pointer(33));
        Require(state.vfunc_14(target,manager) && target.GetPendingHandleForAnalysis()==Pointer(33),"empty Copy");
        state.vfunc_40_ResetForAnalysis(); Require(!state.GetPendingHandleForAnalysis(),"base reset");
    }
}
int main(int argc,char** argv)
{
    if(argc==15 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[13]{};for(unsigned i=0;i<13;++i)v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),unsigned(v[1]),std::uint32_t(v[2]),v[3],v[4],v[5]!=0,v[6],v[7],unsigned(v[8]),std::uint32_t(v[9]),std::uint8_t(v[10]),std::uint32_t(v[11]),v[12]!=0)<<'\n';return EXIT_SUCCESS;
    }
    CheckLifetime<wxMinotaurMovingState>(0);
    wxMinotaurMovingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);state.SetPendingHandleForAnalysis(Pointer(11));
    Require(state.GetField3CForAnalysis()==0 && state.vfunc_34(0),"constructor clears native byte3C");
    state.SetField3CForAnalysis(1);auto clone=state.Clone();Require(static_cast<wxMinotaurMovingState&>(*clone).GetField3CForAnalysis()==0,"clone clears own byte");
    state.vfunc_40_ResetForAnalysis();Require(state.GetField3CForAnalysis()==1,"base Reset leaves own byte");
    wxMinotaurMovingState copy;copy.SetField3CForAnalysis(0xCC);spCloneManager manager;Require(state.vfunc_14(copy,manager) && copy.GetField3CForAnalysis()==0xCC,"empty Copy leaves target own byte");
    state.SetPendingHandleForAnalysis(Pointer(11));host.word=0x3DCCCCCD;
    Require(state.vfunc_1C(request) && state.GetField3CForAnalysis()==1 && !state.vfunc_34(10),"medium entry resets then establishes mode-zero lock");
    Require((request.packedKey&0x70)==0x40 && host.word==0x3DCCCCCD
        && host.trace.find("start:22:0:0:1")!=std::string::npos,"medium branch clamps0.1 and starts mode-zero");
    host.next=Pointer(33);host.word=0x3F800000;host.trace.clear();state.vfunc_30(request);
    Require(state.GetPendingHandleForAnalysis()==Pointer(22) && host.word==0x3DCCCCCD
        && host.trace.find("start:")==std::string::npos && host.trace.find("query:22")!=std::string::npos,"locked incomplete animation retains pending and clamps motion");
    host.first=host.second=22;host.word=0x3F800000;host.trace.clear();state.vfunc_30(request);
    Require(state.GetPendingHandleForAnalysis()==Pointer(33) && state.GetField3CForAnalysis()==0 && state.vfunc_34(0)
        && !host.first && !host.second && host.trace.find("start:33:1:")!=std::string::npos,"completion consumes both records and selects unlocked mode-one");
    host.word=0x3D4CCCCC;host.trace.clear();state.vfunc_30(request);Require((request.packedKey&0x70)==0 && !host.word,"strict lower threshold clears motion");
    host.word=0x3D4CCCCD;host.trace.clear();state.vfunc_30(request);Require((request.packedKey&0x70)==0x40 && host.word==0x3DCCCCCD,"equality at lower threshold selects middle branch");
    std::cout<<"MinotaurMoving state checks passed\n";
}
