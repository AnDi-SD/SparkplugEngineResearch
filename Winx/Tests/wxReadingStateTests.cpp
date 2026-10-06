#include "Code/wxReadingState.h"
#include "Analysis/Host/wxReadingStateHost.h"
#include "Analysis/PC/wxReadingStateAbi.h"
#include "Analysis/PS2/wxReadingStateAbi.h"
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
    struct Host final:wxReadingStateHost
    {
        wxReadingState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;std::uint32_t global=0;
        unsigned receiver=1,mutation=0,predicate=0;std::string trace;
        Host(wxReadingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        std::uint32_t GlobalWord1B0ForAnalysis() override{Event("global");return global;}
        void* MainReceiverForAnalysis() override{Event("receiver");return receiver?Pointer(0x400):nullptr;}
        void SendReadingEntryForAnalysis(wxCharacterState& source) override{Check(&source==&state,"entry source");Event("entry");if(mutation==1){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(77));}}
        void SendReadingExitForAnalysis(void* r,wxCharacterState& source,bool firstPhase) override{Check(r==Pointer(0x400)&&&source==&state,"exit source/receiver");Event("exit:"+std::to_string(firstPhase));if(mutation==2){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(77));}}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));if(mutation==3)state.SetPendingHandleForAnalysis(Pointer(77));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate!=0;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==4)state.SetPendingHandleForAnalysis(Pointer(99));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));if(mutation==5){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(99));}}
        void FadeAnimationForAnalysis(void*,void* h,float duration) override{Check(duration==0.4f,"fade duration");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"query consume");Event("query:"+std::to_string(Token(h)));if(!h)return true;const bool matched=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void*) override{throw std::logic_error("Reading own methods never clear control");}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,
        unsigned flags,std::uint32_t global,unsigned receiver,unsigned mutation,unsigned predicate)
    {
        wxReadingState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.global=global;host.receiver=receiver;host.mutation=mutation;host.predicate=predicate;
        unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x20)result=state.vfunc_20(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxReadingState state;Check(state.IsExactly(wxReadingState::ClassID)&&state.IsKindOf(wxCharacterState::ClassID)&&state.GetStateSelectorForAnalysis()==27,"RTTI/base/selector");
        Check(dynamic_cast<wxReadingState*>(spRTTIManager::Instance().Create(wxReadingState::ClassID).get()),"factory");
        state.SetPendingHandleForAnalysis(Pointer(11));state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone();auto* fresh=dynamic_cast<wxReadingState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone defaults");
        wxReadingState target;target.SetPendingHandleForAnalysis(Pointer(77));spCloneManager manager;Check(state.vfunc_14(target,manager)&&target.GetPendingHandleForAnalysis()==Pointer(77),"Copy empty");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==13&&std::string(argv[1])=="--case")
        {std::uint64_t v[11]{};for(unsigned i=0;i<11;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),std::uint32_t(v[7]),unsigned(v[8]),unsigned(v[9]),unsigned(v[10]))<<'\n';return 0;}
        Lifecycle();wxReadingState unbound;bool threw=false;try{(void)unbound.vfunc_34(0);}catch(const std::logic_error&){threw=true;}Check(threw,"global permission requires host");
        Check(Case(0x1c,0xffffffff,11,22,0,0,31,0,1,1,0).find("entry@11:31:4294967295")!=std::string::npos,"entry before request changes");
        Check(Case(0x30,0xffffffff,22,22,0,0,31,0,1,0,0).find("start:")==std::string::npos,"same update handle untouched");
        Check(Case(0x20,0,11,22,11,11,0,0,1,0,1).find("exit:0")!=std::string::npos,"completed exit second notification");
        Check(Case(0x34,0,11,22,0,0,31,0x47,1,0,0).rfind("0 ",0)==0,"permission blocks global47");
        std::cout<<"PASS Reading lifecycle, phased transitions and global permission\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
