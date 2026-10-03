#include "Code/wxIceWormMovingState.h"
#include "Analysis/Host/wxIceWormMovingStateHost.h"
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
    struct Host final : wxIceWormMovingStateHost
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
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey);
        }
        bool ps2=false,hasTurn=false,reload=false,hasReceiver=false;
        std::uint32_t angleFirst=0,angleSecond=0,normalized=0;
        static float Float(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4); return value; }
        static std::uint32_t Bits(float value) { std::uint32_t bits; std::memcpy(&bits,&value,4); return bits; }
        wxIceWormMovingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept override
        { return ps2?wxIceWormMovingNumericProfileForAnalysis::PS2Finite:wxIceWormMovingNumericProfileForAnalysis::PC; }
        wxIceWormMovingObjectsForAnalysis OwnerEntityObjectsForAnalysis(void*) override
        { Event("objects"); return {Pointer(0x500),hasTurn?Pointer(0x600):nullptr}; }
        void* ReadOwnerTurnObjectForAnalysis(void*) override
        { Event("turn-again"); return Pointer(reload?0x700:0x600); }
        float ReadMotionForAnalysis(void* object) override
        { Require(object==Pointer(0x500),"captured motion object"); Event("motion:"+std::to_string(word)); return Float(word); }
        float ReadTurnWordForAnalysis(void* object,unsigned offset) override
        {
            Require(offset==0x164 || offset==0x1A0,"only native angle fields");
            const bool other=object==Pointer(0x700);
            const auto bits=(offset==0x164)!=other?angleFirst:angleSecond;
            Event("turn:"+std::to_string(offset)+':'+std::to_string(bits));return Float(bits);
        }
        void NormalizeAngleForAnalysis(float& delta) override
        {
            // Fixture of a declared borrowed helper, not a runtime fallback.
            Event("normalize:"+std::to_string(Bits(delta)));delta=Float(normalized);
            Event("normalize-result:"+std::to_string(Bits(delta)));
        }
        void* OwnerField24ForAnalysis(void*) override { Event("owner24"); return hasReceiver?Pointer(0x400):nullptr; }
        void SendMovingNotificationForAnalysis(void* receiver,wxCharacterState& source,unsigned code,unsigned payload) override
        { Require(receiver==Pointer(0x400) && &source==&state && payload==0x6E,"complete moving packet"); Event("notify:"+std::to_string(code)+':'+std::to_string(payload)); }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override { Event("reset:"+std::to_string(Token(handle))); }
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
        std::uintptr_t next,bool predicate,std::uint32_t angleFirst,std::uint32_t angleSecond,
        std::uint32_t motion,std::uint32_t normalized,bool receiver,unsigned flags,bool reload)
    {
        wxIceWormMovingState state; wxAnimationRequestForAnalysis request{key}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=Pointer(next);host.predicate=predicate;host.ps2=profile==1;host.hasTurn=profile>=2 || profile==1;
        host.angleFirst=angleFirst;host.angleSecond=angleSecond;host.word=motion;host.normalized=normalized;host.hasReceiver=receiver;host.reload=reload;
        unsigned result=2;
        if(slot==0x1C) result=state.vfunc_1C(request);
        else if(slot==0x20) result=state.vfunc_20(request);
        else if(slot==0x30) state.vfunc_30(request);
        else throw std::logic_error("unknown worm slot");
        std::ostringstream output;output<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())
            <<' '<<Flags(state)<<'|'<<host.trace;return output.str();
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
        std::uint64_t v[13]{};for(unsigned i=0;i<13;++i) v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),unsigned(v[1]),std::uint32_t(v[2]),v[3],v[4],v[5]!=0,
            std::uint32_t(v[6]),std::uint32_t(v[7]),std::uint32_t(v[8]),std::uint32_t(v[9]),v[10]!=0,unsigned(v[11]),v[12]!=0)<<'\n';
        return EXIT_SUCCESS;
    }
    CheckLifetime<wxIceWormMovingState>(0);
    wxIceWormMovingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);state.SetPendingHandleForAnalysis(Pointer(11));
    host.word=0x3E4CCCCD;host.hasReceiver=true;
    Require(state.vfunc_1C(request) && (request.packedKey&0x70)==0x10,"entry and motion equality");
    Require(host.trace.find("notify:10193:110")<host.trace.find("stop:11")
        && host.trace.find("stop:11")<host.trace.find("objects")
        && host.trace.find("start:22:1:0:1@0:")!=std::string::npos,"notification before base release and mode-one queue before store");
    host.trace.clear();state.vfunc_30(request);
    Require(host.trace.find("lookup:")!=std::string::npos && host.trace.find("start:")==std::string::npos,"same handle still reads input and resolves lookup");
    host.trace.clear();Require(state.vfunc_20(request),"exit completion");
    Require(host.trace.find("notify:10194:110")<host.trace.find("stop:22"),"exit notification precedes release");
    state.SetPendingHandleForAnalysis(Pointer(11));host.hasTurn=true;host.reload=true;host.angleFirst=0x3F800000;host.angleSecond=0;host.normalized=0xBF800000;
    request.packedKey=0xFFFFFFFF;host.trace.clear();state.vfunc_30(request);
    Require((request.packedKey&0x70)==0x30 && host.trace.find("turn-again")!=std::string::npos
        && host.trace.find("motion:")==std::string::npos && host.trace.find("normalize:3212836864")!=std::string::npos,"angular path rereads turn and preserves spilled input");
    wxCharacterStateHost* ordinary=nullptr;state.SetBindingsForAnalysis(Pointer(1),Pointer(2),ordinary);
    bool threw=false;try{state.vfunc_30(request);}catch(const std::logic_error&){threw=true;}Require(threw,"missing host is explicit");
    std::cout<<"IceWormMoving state checks passed\n";
}
