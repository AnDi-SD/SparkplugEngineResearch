#include "Code/wxOpenSecretPassageState.h"
#include "Analysis/Host/wxOpenSecretPassageStateHost.h"
#include "Analysis/PC/wxOpenSecretPassageStateAbi.h"
#include "Analysis/PS2/wxOpenSecretPassageStateAbi.h"
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
    const char* Tags[]={"SND_INTERACTION","SND_INTERACTION_extra","SND_INTERACTIO","snd_interaction","prefix_SND_INTERACTION","","event_damage"};
    struct Host final:wxOpenSecretPassageStateHost
    {
        wxOpenSecretPassageState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;unsigned mutation=0,tag=0;
        std::uint32_t word=0x12345678;std::string trace;
        Host(wxOpenSecretPassageState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        const char* EventTagNameForAnalysis(const void*) override{Event("tag");Check(tag<7,"tag index");return Tags[tag];}
        void* OwnerField124ForAnalysis(void* owner) override{Check(owner==Pointer(0x100),"owner graph");Event("owner124");return mutation?Pointer(0x400):nullptr;}
        void SendFilteredNotificationForAnalysis(wxCharacterState& source,std::uint32_t code,std::uint32_t filter,void* payload0,std::uint32_t payload1) override
        {Check(&source==&state&&code==0x2762&&filter==6&&payload0==(mutation?Pointer(0x400):nullptr)&&payload1==0,"native filtered packet");Event("message:"+std::to_string(code)+':'+std::to_string(filter)+':'+std::to_string(Token(payload0))+':'+std::to_string(payload1));}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return false;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {Check(consume,"consume completion");Event("query:"+std::to_string(Token(h)));if(!h)return true;const auto v=Token(h);const bool matched=first==v||second==v;if(first==v)first=0;if(second==v)second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void* owner) override
        {Check(owner==Pointer(0x100),"direct owner control");Event("word");word=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned mutation)
    {
        wxOpenSecretPassageState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
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
        wxOpenSecretPassageState source;Check(source.IsExactly(wxOpenSecretPassageState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==41,"physical base/RTTI/selector");
        Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==31,"fresh fields");
        Check(dynamic_cast<wxOpenSecretPassageState*>(spRTTIManager::Instance().Create(wxOpenSecretPassageState::ClassID).get()),"factory");
        source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=source.Clone();auto* fresh=dynamic_cast<wxOpenSecretPassageState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone fresh state");
        wxOpenSecretPassageState target;target.SetPendingHandleForAnalysis(Pointer(7));spCloneManager manager;Check(source.vfunc_14(target,manager)&&Token(target.GetPendingHandleForAnalysis())==7,"Copy no-op");
        source.vfunc_40_ResetForAnalysis();Check(!source.GetPendingHandleForAnalysis()&&Flags(source)==7,"base reset restores first three flags");
        Check(source.vfunc_34(0),"native null completion shortcut needs no host");
        wxAnimationRequestForAnalysis request{};bool threw=false;try{source.vfunc_30(request);}catch(const std::logic_error&){threw=true;}Check(threw,"missing host is explicit");
    }
    void DirectEntry()
    {
        struct Derived final:wxOpenSecretPassageState
        {unsigned updates=0;void vfunc_30(wxAnimationRequestForAnalysis&) override{++updates;}};
        Derived state;wxAnimationRequestForAnalysis request{0xffffffff};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        Check(state.vfunc_1C(request)&&!state.updates&&host.word==0,"entry control clear is direct");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==10&&std::string(argv[1])=="--case")
        {std::uint64_t v[8]{};for(unsigned i=0;i<8;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]))<<'\n';return 0;}
        Lifecycle();DirectEntry();
        Check(Case(0x1c,0xffffffff,22,22,22,22,31,0).find("reset:22")!=std::string::npos,"entry always queues same handle");
        Check(Case(0x34,9,0,22,11,22,31,0).find("query:")==std::string::npos,"null completion shortcut");
        Check(Case(0x3c,0,11,22,1,0,31,0).find("owner124")==std::string::npos,"full event equality");
        std::cout<<"PASS OpenSecretPassage lifecycle, entry, control and notification operations\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
