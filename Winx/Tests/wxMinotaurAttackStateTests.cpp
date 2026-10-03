#include "Code/wxMinotaurAttackState.h"
#include "Analysis/Host/wxMinotaurAttackStateHost.h"
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
    struct Host final : wxMinotaurAttackStateHost
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
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+std::to_string(static_cast<wxMinotaurAttackState&>(state).GetField3CForAnalysis());
        }
        bool ps2=false,receiver=true;unsigned exitFlag=0;std::uint32_t numerator=0x3F800000,denominator=0x3F800000,speed=0;
        wxMinotaurAttackNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept override
        { return ps2?wxMinotaurAttackNumericProfileForAnalysis::PS2Finite:wxMinotaurAttackNumericProfileForAnalysis::PC; }
        static float Float(unsigned bits) {float value;std::memcpy(&value,&bits,4);return value;}
        static unsigned Bits(float value) {unsigned bits;std::memcpy(&bits,&value,4);return bits;}
        void* OwnerSpeedObjectForAnalysis(void*) override {Event("speedobject");return Pointer(0x500);}
        float ReadSpeedNumeratorForAnalysis(void* object) override {Require(object==Pointer(0x500),"borrowed speed object");Event("numerator");return Float(numerator);}
        float ReadSpeedDenominatorForAnalysis(void* object) override {Require(object==Pointer(0x500),"borrowed speed object");Event("denominator");return Float(denominator);}
        std::uint8_t OwnerExitFlag60ForAnalysis(void*) override {Event("exitflag");return std::uint8_t(exitFlag);}
        void* OwnerField24ForAnalysis(void*) override {Event("owner24");return receiver?Pointer(0x400):nullptr;}
        void SendFlagNotificationForAnalysis(void* target,wxCharacterState& source,bool flag) override {Require(target==Pointer(0x400) && &source==&state,"borrowed flag packet");Event("flag:"+std::to_string(flag));}
        void SetConsumerSpeedForAnalysis(void* consumer,float value) override {Require(consumer==Pointer(0x200),"borrowed consumer");Event("speed:"+std::to_string(Bits(value)));speed=Bits(value);}
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
        wxMinotaurAttackState state;wxAnimationRequestForAnalysis request{std::uint32_t(v[2])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[3]));
        state.SetTransitionFlagsForAnalysis(v[8]&1,v[8]&2,v[8]&4,v[8]&8,v[8]&16);state.SetField3CForAnalysis(std::uint8_t(v[10]));
        host.ps2=v[0]!=0;host.next=Pointer(v[4]);host.predicate=v[5]!=0;host.first=v[6];host.second=v[7];host.word=unsigned(v[9]);host.numerator=unsigned(v[11]);host.denominator=unsigned(v[12]);host.exitFlag=unsigned(v[13]);host.receiver=v[14]!=0;host.speed=unsigned(v[15]);
        unsigned result=2;
        if(v[1]==0x1C)result=state.vfunc_1C(request);
        else if(v[1]==0x20)result=state.vfunc_20(request);
        else if(v[1]==0x30)state.vfunc_30(request);
        else throw std::logic_error("minotaur attack slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<unsigned(state.GetField3CForAnalysis())<<' '<<host.speed<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==18 && std::string(argv[1])=="--case")
    {std::uint64_t v[16]{};for(unsigned i=0;i<16;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxMinotaurAttackState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(state.GetStateSelectorForAnalysis()==3 && !state.GetField3CForAnalysis(),"constructor");
    Require(dynamic_cast<wxMinotaurAttackState*>(spRTTIManager::Instance().Create(wxMinotaurAttackState::ClassID).get()),"factory");
    state.SetField3CForAnalysis(1);state.SetPendingHandleForAnalysis(Pointer(11));host.word=7;
    Require(!state.vfunc_1C(request) && !state.GetField3CForAnalysis() && state.GetPendingHandleForAnalysis()==Pointer(22) && !(Flags(state)&1) && !host.word,"entry first phase");
    host.first=host.second=22;host.numerator=Host::Bits(0.4f);host.trace.clear();
    Require(state.vfunc_1C(request) && request.packedKey==0xF0000450 && host.speed==Host::Bits(0.8f),"entry completion proceeds to base and speed update");
    Require(host.trace.find("flag:1")<host.trace.find("speedobject"),"notification before base virtual update");
    state.SetField3CForAnalysis(1);host.trace.clear();Require(!state.vfunc_20(request) && !(Flags(state)&4) && request.packedKey==0xF0C00450 && host.speed==Host::Bits(1.0f),"exit first phase restores speed and chooses own-byte variant");
    host.first=host.second=22;host.trace.clear();Require(state.vfunc_20(request) && !state.GetPendingHandleForAnalysis() && host.trace.find("flag:0")!=std::string::npos,"exit completed phase notifies before release");
    state.vfunc_40_ResetForAnalysis();Require(state.GetField3CForAnalysis()==1,"base reset leaves own byte");
    state.SetPendingHandleForAnalysis(Pointer(22));host.exitFlag=1;Require(state.vfunc_20(request) && !state.GetPendingHandleForAnalysis() && (Flags(state)&4),"external exit override leaves once flag");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxMinotaurAttackState*>(clone.get());Require(fresh && !fresh->GetField3CForAnalysis() && Flags(*fresh)==31 && !fresh->GetPendingHandleForAnalysis(),"fresh clone");
    wxMinotaurAttackState target;target.SetField3CForAnalysis(1);spCloneManager manager;Require(state.vfunc_14(target,manager) && target.GetField3CForAnalysis()==1,"empty Copy");
    std::cout<<"MinotaurAttack checks passed\n";
}
