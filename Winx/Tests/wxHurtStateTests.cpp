#include "Code/wxHurtState.h"
#include "Analysis/Host/wxHurtStateHost.h"
#include "Analysis/PC/wxHurtStateAbi.h"
#include "Analysis/PS2/wxHurtStateAbi.h"
#include <cstring>
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
    float Float(std::uint32_t bits){float v;std::memcpy(&v,&bits,4);return v;}
    unsigned Flags(const wxCharacterState& s){unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v;}
    struct Host final:wxHurtStateHost
    {
        wxHurtState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;std::uint32_t motionBits=0,mode=0,word=0x12345678;
        unsigned state60=0,predicate=0,mutation=0,profile=0,motionReads=0;std::string trace;
        Host(wxHurtState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+std::to_string(state.GetByte3CForAnalysis());}
        wxHurtStatePlatformForAnalysis PlatformForAnalysis() const noexcept override{return profile?wxHurtStatePlatformForAnalysis::PS2:wxHurtStatePlatformForAnalysis::PC;}
        void* OwnerHurtControlForAnalysis(void*) override{Event("control");return Pointer(0x500);}
        std::uint8_t HurtControlByte60ForAnalysis(void* c) override{Check(c==Pointer(0x500),"captured control");Event("byte60");return static_cast<std::uint8_t>(state60);}
        float HurtControlMotionForAnalysis(void* c) override{Check(c==Pointer(0x500),"captured motion control");Event("motion");++motionReads;return mutation==3&&motionReads>1?Float(0x3e99999a):Float(motionBits);}
        std::uint32_t OwnerModeForAnalysis(void*) override{Event("mode");return mode;}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));if(mutation==1)state.SetPendingHandleForAnalysis(Pointer(77));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate!=0;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool playMode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(playMode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==2)state.SetPendingHandleForAnalysis(Pointer(99));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"consume query");Event("query:"+std::to_string(Token(h)));if(!h)return true;bool match=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return match;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,
        unsigned flags,unsigned byte,unsigned state60,std::uint32_t motionBits,std::uint32_t mode,unsigned predicate,unsigned mutation,unsigned profile)
    {
        wxHurtState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);state.SetByte3CForAnalysis(static_cast<std::uint8_t>(byte));
        host.next=next;host.first=first;host.second=second;host.state60=state60;host.motionBits=motionBits;host.mode=mode;host.predicate=predicate;host.mutation=mutation;host.profile=profile;
        unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<unsigned(state.GetByte3CForAnalysis())<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxHurtState source;Check(source.IsExactly(wxHurtState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID)&&source.GetStateSelectorForAnalysis()==10,"RTTI/base/selector");Check(!source.GetByte3CForAnalysis(),"own byte default");
        Check(dynamic_cast<wxHurtState*>(spRTTIManager::Instance().Create(wxHurtState::ClassID).get()),"factory");
        source.SetByte3CForAnalysis(255);source.SetPendingHandleForAnalysis(Pointer(11));auto clone=source.Clone();auto* fresh=dynamic_cast<wxHurtState*>(clone.get());Check(fresh&&!fresh->GetByte3CForAnalysis()&&!fresh->GetPendingHandleForAnalysis(),"clone own default");
        wxHurtState target;target.SetByte3CForAnalysis(7);spCloneManager manager;Check(source.vfunc_14(target,manager)&&target.GetByte3CForAnalysis()==7,"copy empty");source.vfunc_40_ResetForAnalysis();Check(source.GetByte3CForAnalysis()==255,"reset preserves own byte");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==16&&std::string(argv[1])=="--case")
        {std::uint64_t v[14]{};for(unsigned i=0;i<14;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),unsigned(v[8]),std::uint32_t(v[9]),std::uint32_t(v[10]),unsigned(v[11]),unsigned(v[12]),unsigned(v[13]))<<'\n';return 0;}
        Lifecycle();wxHurtState unbound;Check(unbound.vfunc_34(0),"null permission before host");bool threw=false;try{(void)unbound.vfunc_38(0);}catch(const std::logic_error&){threw=true;}Check(threw,"transition permission missing host error");
        Check(Case(0x38,4,11,22,0,0,31,0,0,0,4,0,0,0).rfind("0 ",0)==0,"mode4 blocks code4");
        Check(Case(0x1c,0x87654320,11,22,11,22,31,1,1,0,0,0,0,0).find("byte60")!=std::string::npos,"own byte special gate");
        Check(Case(0x1c,0xf0000006,11,22,11,22,31,255,255,0x7fc00000,0,0,0,0).find("motion")==std::string::npos,"kind6 bypasses motion reads");
        Check(Case(0x1c,0,11,22,11,22,31,0,0,0x3f4ccccd,0,0,3,0)!=Case(0x1c,0,11,22,11,22,31,0,0,0x3f4ccccd,0,0,3,1),"PC second read versus PS2 cached scalar");
        std::cout<<"PASS Hurt lifecycle, key branches and transition permissions\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
