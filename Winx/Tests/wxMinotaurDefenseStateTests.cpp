#include "Code/wxMinotaurDefenseState.h"
#include "Analysis/Host/wxMinotaurDefenseStateHost.h"
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
    struct Host final : wxMinotaurDefenseStateHost
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
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+std::to_string(static_cast<wxMinotaurDefenseState&>(state).GetField3CForAnalysis());
        }
        void* OwnerControlObjectForAnalysis(void*) override {Event("controlobject");return Pointer(0x600);}
        float ReadControlMotionForAnalysis(void* control) override
        {Require(control==Pointer(0x600),"captured control");Event("motion");float value;std::memcpy(&value,&word,4);return value;}
        void WriteControlWordForAnalysis(void* control,unsigned bits) override
        {Require(control==Pointer(0x600),"captured control write");Event("word");word=bits;}
        bool receiver=true;
        void* OwnerField24ForAnalysis(void*) override {Event("owner24");return receiver?Pointer(0x400):nullptr;}
        void SendDefenseNotificationForAnalysis(void* target,wxCharacterState& source,unsigned code) override
        {Require(target==Pointer(0x400) && &source==&state,"borrowed packet");Event("notify:"+std::to_string(code));}
        void InvokeDefenseControllerForAnalysis(void* controller,unsigned a,unsigned b) override
        {Require(controller==Pointer(0x500),"borrowed controller");Event("controller:"+std::to_string(a)+':'+std::to_string(b));}
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
    std::string Case(const std::uint64_t* v)
    {
        wxMinotaurDefenseState state;wxAnimationRequestForAnalysis request{std::uint32_t(v[1])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[2]));
        state.SetTransitionFlagsForAnalysis(v[7]&1,v[7]&2,v[7]&4,v[7]&8,v[7]&16);state.SetField3CForAnalysis(std::uint8_t(v[9]));
        if(v[11])state.SetControllerForAnalysis(Pointer(0x500));
        host.next=Pointer(v[3]);host.predicate=v[4]!=0;host.first=v[5];host.second=v[6];host.word=unsigned(v[8]);host.receiver=v[10]!=0;
        unsigned result=2;
        if(v[0]==0x1C)result=state.vfunc_1C(request);
        else if(v[0]==0x20)result=state.vfunc_20(request);
        else if(v[0]==0x30)state.vfunc_30(request);
        else throw std::logic_error("shadow defense slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<unsigned(state.GetField3CForAnalysis())<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==14 && std::string(argv[1])=="--case")
    {std::uint64_t v[12]{};for(unsigned i=0;i<12;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxMinotaurDefenseState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(state.GetStateSelectorForAnalysis()==8 && !state.GetField3CForAnalysis() && !state.GetControllerForAnalysis(),"constructor leaves word40 unknown");
    Require(dynamic_cast<wxMinotaurDefenseState*>(spRTTIManager::Instance().Create(wxMinotaurDefenseState::ClassID).get()),"factory");
    state.SetField3CForAnalysis(1);state.SetPendingHandleForAnalysis(Pointer(11));host.word=7;
    Require(!state.vfunc_1C(request) && !state.GetField3CForAnalysis() && !(Flags(state)&1) && !host.word,"entry first phase");
    Require(host.trace.find("start:22:0:0:1@0:31:4028629376:1")!=std::string::npos,"playback observes own byte before clear");
    state.SetControllerForAnalysis(Pointer(0x500));host.first=host.second=22;host.trace.clear();
    Require(state.vfunc_1C(request) && request.packedKey==0xF0000180,"entry completed phase dispatches controller and base update");
    Require(host.trace.find("notify:10193")<host.trace.find("controller:1:1") && host.trace.find("controller:1:1")<host.trace.find("lookup:"),"entry notification/controller/update order");
    host.trace.clear();Require(!state.vfunc_20(request) && !(Flags(state)&4) && request.packedKey==0xF0400180,"exit first phase");
    Require(host.trace.find("notify:10194")<host.trace.find("controller:0:1") && host.trace.find("controller:0:1")<host.trace.find("stop:"),"exit packet/controller before release");
    host.first=host.second=22;Require(state.vfunc_20(request) && !state.GetPendingHandleForAnalysis(),"exit completed phase");
    state.SetField3CForAnalysis(1);state.vfunc_40_ResetForAnalysis();Require(state.GetField3CForAnalysis()==1 && state.GetControllerForAnalysis()==Pointer(0x500),"base reset leaves own fields");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxMinotaurDefenseState*>(clone.get());Require(fresh && !fresh->GetField3CForAnalysis() && !fresh->GetControllerForAnalysis() && Flags(*fresh)==31,"fresh clone leaves word40 unknown");
    wxMinotaurDefenseState target;target.SetControllerForAnalysis(Pointer(0x700));target.SetField3CForAnalysis(1);spCloneManager manager;Require(state.vfunc_14(target,manager) && target.GetControllerForAnalysis()==Pointer(0x700) && target.GetField3CForAnalysis()==1,"empty Copy");
    wxMinotaurDefenseState unset;Host unsetHost(unset,request);unset.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&unsetHost);bool rejected=false;try{(void)unset.vfunc_20(request);}catch(const std::logic_error&){rejected=true;}Require(rejected,"uninitialized controller is a required binding");
    std::cout<<"MinotaurDefense checks passed\n";
}
