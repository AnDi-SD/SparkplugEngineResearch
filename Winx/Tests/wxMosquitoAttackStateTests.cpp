#include "Code/wxMosquitoAttackState.h"
#include "Analysis/Host/wxMosquitoAttackStateHost.h"
#include "Analysis/PC/wxMosquitoAttackStateAbi.h"
#include "Analysis/PS2/wxMosquitoAttackStateAbi.h"
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
    unsigned Flags(const wxCharacterState& s){unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v;}
    const char* Tags[]={"event_shoot","event_shoot_extra","event_shoo","Event_shoot","prefix_event_shoot","","event_damage"};
    struct Host final:wxMosquitoAttackStateHost
    {
        wxMosquitoAttackState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned mutation=0,tag=0;
        std::uint32_t word=0x12345678;std::string trace;
        Host(wxMosquitoAttackState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        const char* EventTagNameForAnalysis(const void*) override{Event("tag");Check(tag<7,"tag index");return Tags[tag];}
        void ResetOwnerEntityControllerForAnalysis(void* owner) override{Event("entity-reset:"+std::to_string(Token(owner)));}
        void CallOwnerEntityField140Slot38ForAnalysis(void* owner,bool value) override{Event("entity-slot:"+std::to_string(Token(owner))+':'+std::to_string(value));}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return false;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {Check(consume,"consume completion");Event("query:"+std::to_string(Token(h)));if(!h)return true;const auto v=Token(h);const bool matched=first==v||second==v;if(first==v)first=0;if(second==v)second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void* owner) override
        {Check(owner==Pointer(0x100),"direct owner control");Event("word");word=0;if(mutation==1)state.SetBindingsForAnalysis(Pointer(0x101),Pointer(0x200),this);}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned mutation)
    {
        wxMosquitoAttackState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);host.next=next;host.first=first;host.second=second;host.mutation=mutation;host.tag=unsigned(first);
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
        wxMosquitoAttackState source;Check(source.IsExactly(wxMosquitoAttackState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==3,"physical base/RTTI/selector");
        Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"fresh fields");
        Check(dynamic_cast<wxMosquitoAttackState*>(spRTTIManager::Instance().Create(wxMosquitoAttackState::ClassID).get()),"factory");
        source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=source.Clone();auto* fresh=dynamic_cast<wxMosquitoAttackState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone fresh state");
        wxMosquitoAttackState target;target.SetPendingHandleForAnalysis(Pointer(7));spCloneManager manager;Check(source.vfunc_14(target,manager)&&Token(target.GetPendingHandleForAnalysis())==7,"Copy no-op");
        source.vfunc_40_ResetForAnalysis();Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==7,"base reset restores first three flags");
        wxAnimationRequestForAnalysis request{};bool threw=false;try{source.vfunc_30(request);}catch(const std::logic_error&){threw=true;}Check(threw,"missing host is explicit");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==10&&std::string(argv[1])=="--case")
        {std::uint64_t v[8]{};for(unsigned i=0;i<8;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]))<<'\n';return 0;}
        Lifecycle();
        Check(Case(0x1c,0xffffffff,22,22,22,22,31,0).find("reset:22")!=std::string::npos,"entry always queues same handle");
        Check(Case(0x30,0,11,22,0,0,31,1).find("entity-reset:257")!=std::string::npos,"owner reload after direct clear");
        Check(Case(0x34,9,0,22,11,22,31,0).find("query:0")!=std::string::npos,"null completion still queried");
        Check(Case(0x3c,0,11,22,1,0,31,0).find("entity-slot:")==std::string::npos,"full event equality");
        std::cout<<"PASS MosquitoAttack lifecycle, entry, control reset and event operations\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
