#include "Code/wxIceWormAttackState.h"
#include "Analysis/Host/wxIceWormAttackStateHost.h"
#include "Analysis/PC/wxIceWormAttackStateAbi.h"
#include "Analysis/PS2/wxIceWormAttackStateAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
    void* Pointer(std::uintptr_t v){return reinterpret_cast<void*>(v);}
    std::uintptr_t Token(void* v){return reinterpret_cast<std::uintptr_t>(v);}
    unsigned Flags(const wxCharacterState& s){unsigned word=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])word|=1u<<i;return word;}
    struct Host final:wxIceWormAttackStateHost
    {
        wxIceWormAttackState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;std::uint32_t global=0;
        unsigned receiver=1,mutation=0,predicate=0,eventCode=0;std::string trace;
        Host(wxIceWormAttackState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        std::uint32_t OwnerPackedWordForAnalysis(void*) override{Event("word");return global;}
        void* OwnerField24ForAnalysis(void* owner) override{Event("receiver:"+std::to_string(Token(owner)));return receiver?Pointer(0x400):nullptr;}
        std::string_view EventTagNameForAnalysis(const void*) override{Event("tag");static constexpr const char* names[]={"event_shoot","event_shootX","event_shoo","EVENT_SHOOT","shoot","","event_shoot ","event_shot"};return names[eventCode];}
        void SendAttackNotificationForAnalysis(void* r,wxCharacterState& source,std::uint32_t code,std::uint32_t payload) override
        {
            Check(r==Pointer(0x400)&&&source==&state,"notification receiver/source");
            Check(payload==(code==0x2758?0u:0x6fu),"packet payload");Event("notify:"+std::to_string(code)+':'+std::to_string(payload));
            if(mutation==1){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(77));}
            if(mutation==6)state.SetBindingsForAnalysis(Pointer(0x300),Pointer(0x200),this);
        }
        void* ResolveAnimationForAnalysis(void* owner,std::uint32_t key) override{Event("lookup:"+std::to_string(key)+":"+std::to_string(Token(owner)));if(mutation==3)state.SetPendingHandleForAnalysis(Pointer(77));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate!=0;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==4)state.SetPendingHandleForAnalysis(Pointer(99));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));if(mutation==5){request.packedKey=0x500;state.SetPendingHandleForAnalysis(Pointer(99));}}
        void FadeAnimationForAnalysis(void*,void* h,float duration) override{Check(duration==0.4f,"fade duration");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"query consume");Event("query:"+std::to_string(Token(h)));if(!h)return true;const bool matched=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void*) override{throw std::logic_error("IceWormAttack own methods never clear control");}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,
        unsigned flags,std::uint32_t global,unsigned receiver,unsigned mutation,unsigned predicate,unsigned eventCode)
    {
        wxIceWormAttackState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.global=global;host.receiver=receiver;host.mutation=mutation;host.predicate=predicate;host.eventCode=eventCode;
        unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x20)result=state.vfunc_20(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x3c)state.vfunc_3C(nullptr);
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxIceWormAttackState state;Check(state.IsExactly(wxIceWormAttackState::ClassID)&&state.IsKindOf(wxCharacterState::ClassID)&&state.GetStateSelectorForAnalysis()==3,"RTTI/base/selector");
        Check(dynamic_cast<wxIceWormAttackState*>(spRTTIManager::Instance().Create(wxIceWormAttackState::ClassID).get()),"factory");
        state.SetPendingHandleForAnalysis(Pointer(11));state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone();auto* fresh=dynamic_cast<wxIceWormAttackState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone defaults");
        wxIceWormAttackState target;target.SetPendingHandleForAnalysis(Pointer(77));spCloneManager manager;Check(state.vfunc_14(target,manager)&&target.GetPendingHandleForAnalysis()==Pointer(77),"Copy empty");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==14&&std::string(argv[1])=="--case")
        {std::uint64_t v[12]{};for(unsigned i=0;i<12;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),std::uint32_t(v[7]),unsigned(v[8]),unsigned(v[9]),unsigned(v[10]),unsigned(v[11]))<<'\n';return 0;}
        Lifecycle();wxIceWormAttackState unbound;bool threw=false;try{(void)unbound.vfunc_34(0);}catch(const std::logic_error&){threw=true;}Check(threw,"permission requires owner host");
        Check(Case(0x1c,0xffffffff,11,22,0,0,31,0,1,1,0,0).find("notify:10193:111@11:31:4294967295")!=std::string::npos,"entry notification before key mutation");
        Check(Case(0x30,0,22,22,0,0,31,0,1,0,0,0).find("start:")==std::string::npos,"same update handle untouched");
        Check(Case(0x20,0,11,22,11,11,0,0,1,0,1,0).find("notify:10194:111")!=std::string::npos,"completed exit notification");
        Check(Case(0x34,0,11,22,0,0,31,0x400,1,0,0,0).rfind("0 ",0)==0,"permission consumes completion for selector8");
        std::cout<<"PASS IceWormAttack lifecycle, phased transitions, release ordering and events\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
