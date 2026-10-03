#include "Code/wxFrogAttackState.h"
#include "Analysis/Host/wxFrogAttackStateHost.h"
#include "Analysis/PC/wxFrogAttackStateAbi.h"
#include "Analysis/PS2/wxFrogAttackStateAbi.h"
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
    const char* Tags[]={"event_damage_end","event_damage_end_extra","event_damage_en","Event_damage_end","prefix_event_damage_end","","event_damage"};
    struct Host final:wxFrogAttackStateHost
    {
        wxFrogAttackState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned receiver=1,mutation=0,tag=0;
        std::uint32_t word=0x12345678;std::string trace;
        Host(wxFrogAttackState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+std::to_string(state.GetByte3CForAnalysis());}
        const char* EventTagNameForAnalysis(const void*) override{Event("tag");Check(tag<7,"tag index");return Tags[tag];}
        void* OwnerField24ForAnalysis(void*) override{Event("owner24");return receiver?Pointer(0x400):nullptr;}
        void RestartReverseForAnalysis(void*,void* h) override{Event("reverse:"+std::to_string(Token(h)));if(mutation==1)receiver=0;if(mutation==3)state.SetPendingHandleForAnalysis(Pointer(99));}
        void SendUnnamedFlagNotificationForAnalysis(void* r,wxCharacterState& source,bool flag) override
        {Check(r==Pointer(0x400)&&&source==&state,"flag packet receiver/source");Event("notify:10015:"+std::to_string(flag));if(flag&&mutation==2){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(77));}}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return false;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {Check(consume,"consume completion");Event("query:"+std::to_string(Token(h)));if(!h)return true;const auto v=Token(h);const bool matched=first==v||second==v;if(first==v)first=0;if(second==v)second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned byte,unsigned receiver,unsigned mutation)
    {
        wxFrogAttackState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetByte3CForAnalysis(static_cast<std::uint8_t>(byte));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);host.next=next;host.first=first;host.second=second;host.receiver=receiver;host.mutation=mutation;host.tag=unsigned(first);
        unsigned result=2;
        if(slot==0x0c)state.vfunc_0C(&key);
        else if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x3c)state.vfunc_3C(Pointer(0x300));
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("case slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<unsigned(state.GetByte3CForAnalysis())<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxFrogAttackState source;Check(source.IsExactly(wxFrogAttackState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==3,"physical base/RTTI/selector");
        Check(!source.GetByte3CForAnalysis()&&!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"fresh fields");
        Check(dynamic_cast<wxFrogAttackState*>(spRTTIManager::Instance().Create(wxFrogAttackState::ClassID).get()),"factory");
        source.SetByte3CForAnalysis(255);source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=source.Clone();auto* fresh=dynamic_cast<wxFrogAttackState*>(clone.get());Check(fresh&&!fresh->GetByte3CForAnalysis()&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone fresh state");
        wxFrogAttackState target;target.SetByte3CForAnalysis(7);spCloneManager manager;Check(source.vfunc_14(target,manager)&&target.GetByte3CForAnalysis()==7,"Copy no-op");
        source.vfunc_40_ResetForAnalysis();Check(source.GetByte3CForAnalysis()==255&&!source.GetPendingHandleForAnalysis(),"base reset preserves own byte");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==12&&std::string(argv[1])=="--case")
        {std::uint64_t v[10]{};for(unsigned i=0;i<10;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),unsigned(v[8]),unsigned(v[9]))<<'\n';return 0;}
        Lifecycle();
        Check(Case(0x1c,0xffffffff,22,22,22,22,31,0,1,0).find("reset:22")!=std::string::npos,"entry always queues same handle");
        Check(Case(0x0c,0x275c,11,22,0,0,31,0,1,1).find("notify:")==std::string::npos,"receiver read after reverse callback");
        Check(Case(0x34,9,0,22,11,22,31,0,0,0).find("query:0")!=std::string::npos,"null handle completion still queried");
        Check(Case(0x3c,0,11,22,1,0,31,0,1,0).find("owner24")==std::string::npos,"full event equality");
        std::cout<<"PASS FrogAttack lifecycle, entry, notification and completion operations\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
