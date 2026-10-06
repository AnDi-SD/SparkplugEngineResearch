#include "Code/wxSpiderAttackState.h"
#include "Analysis/Host/wxSpiderAttackStateHost.h"
#include "Analysis/PC/wxSpiderAttackStateAbi.h"
#include "Analysis/PS2/wxSpiderAttackStateAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
    void* Pointer(std::uintptr_t value){return reinterpret_cast<void*>(value);}
    std::uintptr_t Token(void* value){return reinterpret_cast<std::uintptr_t>(value);}
    unsigned Flags(const wxCharacterState& state){unsigned word=0;auto f=state.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])word|=1u<<i;return word;}
    const char* Tags[]={"event_impact_begin","event_impact_end","event_web","event_spit","event_web_extra","Event_impact_begin","","event_impact_begi","prefix_event_spit"};
    struct Host final:wxSpiderAttackStateHost
    {
        wxSpiderAttackState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned control=0,receiver=1,mutation=0,tag=0,predicate=0;
        std::uint32_t word=0x12345678;std::string trace;
        Host(wxSpiderAttackState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        void SetConsumerRateForAnalysis(void*,float rate) override{Check(rate==1.0f,"rate1");Event("rate");if(mutation==1){request.packedKey=0xabcdef12;control=255;}}
        std::uint8_t OwnerControlByte20ForAnalysis(void*) override{Event("control20");return static_cast<std::uint8_t>(control);}
        std::string_view EventTagNameForAnalysis(const void*) override{Event("tag");Check(tag<9,"tag index");return Tags[tag];}
        void* OwnerField24ForAnalysis(void*) override{Event("owner24");return receiver?Pointer(0x400):nullptr;}
        void SendImpactNotificationForAnalysis(void* r,wxCharacterState& source,bool begin) override{Check(r==Pointer(0x400)&&&source==&state,"receiver/source");Event("notify:"+std::to_string(begin));}
        void TriggerOwnerActionForAnalysis(void*,std::uint32_t action) override{Event("action:"+std::to_string(action));}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));if(mutation==2)state.SetPendingHandleForAnalysis(Pointer(77));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate!=0;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==3)state.SetPendingHandleForAnalysis(Pointer(99));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"consume completion");Event("query:"+std::to_string(Token(h)));if(!h)return true;const bool match=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return match;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned control,unsigned receiver,unsigned mutation,unsigned predicate)
    {
        wxSpiderAttackState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.control=control;host.receiver=receiver;host.mutation=mutation;host.tag=unsigned(first);host.predicate=predicate;
        unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x3c)state.vfunc_3C(Pointer(0x300));
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("case slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxSpiderAttackState source;Check(source.IsExactly(wxSpiderAttackState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==3,"RTTI/base/selector");
        Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"fresh base");
        Check(dynamic_cast<wxSpiderAttackState*>(spRTTIManager::Instance().Create(wxSpiderAttackState::ClassID).get()),"factory");
        source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=source.Clone();auto* fresh=dynamic_cast<wxSpiderAttackState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone fresh");
        wxSpiderAttackState target;target.SetPendingHandleForAnalysis(Pointer(77));spCloneManager manager;
        Check(source.vfunc_14(target,manager)&&target.GetPendingHandleForAnalysis()==Pointer(77),"Copy empty");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==13&&std::string(argv[1])=="--case")
        {std::uint64_t v[11]{};for(unsigned i=0;i<11;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),unsigned(v[8]),unsigned(v[9]),unsigned(v[10]))<<'\n';return 0;}
        wxSpiderAttackState unbound;Check(unbound.vfunc_34(99),"null permission before host");
        bool threw=false;unbound.SetPendingHandleForAnalysis(Pointer(11));try{(void)unbound.vfunc_34(0);}catch(const std::logic_error&){threw=true;}Check(threw,"unbound nonnull explicit host error");
        Lifecycle();
        unbound.SetPendingHandleForAnalysis(nullptr);
        Check(Case(0x1c,0xffffffff,22,22,22,22,31,255,1,0,1).find("start:22:0:2:1")!=std::string::npos,"always queues same handle with fade2");
        Check(Case(0x1c,0,11,0,11,0,31,0,1,3,0).find("word@0:")!=std::string::npos,"pending stored after callback mutation");
        Check(Case(0x3c,0,11,22,4,0,31,0,1,0,0).find("action:")==std::string::npos,"exact event equality");
        Check(Case(0x3c,0,11,22,2,0,31,0,1,0,0).find("action:0")!=std::string::npos,"web action0");
        std::cout<<"PASS SpiderAttack entry, event and completion operations\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
