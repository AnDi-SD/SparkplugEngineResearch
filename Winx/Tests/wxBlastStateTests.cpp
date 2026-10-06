#include "Code/wxBlastState.h"
#include "Analysis/Host/wxBlastStateHost.h"
#include "Analysis/PC/wxBlastStateAbi.h"
#include "Analysis/PS2/wxBlastStateAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v,const char* label){if(!v)throw std::runtime_error(label);}
    void* Pointer(std::uintptr_t v){return reinterpret_cast<void*>(v);}
    std::uintptr_t Token(void* p){return reinterpret_cast<std::uintptr_t>(p);}
    unsigned Flags(const wxCharacterState& s){unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v;}
    const char* Tags[]={"event_blast","prefix_event_blast_suffix","event_dragon_fire","event_dragon_fire_extra","Event_blast","","event_blas","event_dragon_fire_event_blast"};
    struct Host final:wxBlastStateHost
    {
        wxBlastState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;std::int32_t mode=0;
        unsigned allowed=1,receivers=15,mutation=0,tag=0,profile=0,modeReads=0;
        std::uint32_t word=0x12345678;std::string trace;
        Host(wxBlastState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& text){if(!trace.empty())trace+=';';trace+=text+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        wxBlastStatePlatformForAnalysis PlatformForAnalysis() const noexcept override{return profile?wxBlastStatePlatformForAnalysis::PS2:wxBlastStatePlatformForAnalysis::PC;}
        std::string_view EventTagNameForAnalysis(const void*) override{Check(tag<8,"tag index");Event("tag");return Tags[tag];}
        void* OwnerField24ForAnalysis(void*) override{Event("owner24");return receivers&1?Pointer(0x400):nullptr;}
        std::int32_t GlobalModeForAnalysis() override{Event("mode");++modeReads;return profile&&mutation==6&&modeReads>1?6:mode;}
        void* OwnerResourceObjectForAnalysis(void*) override{Event("resource");return Pointer(0x500);}
        bool ConsumeResourceForAnalysis(void* r,float amount) override{Check(r==Pointer(0x500),"resource token");Event("consume:"+std::to_string(unsigned(amount)));if(mutation==3){request.packedKey=0x87654321;state.SetPendingHandleForAnalysis(Pointer(99));}return allowed!=0;}
        void* OwnerActionObjectForAnalysis(void*) override{Event("actionObject");return Pointer(0x600);}
        void TriggerActionForAnalysis(void* r,std::uint32_t action) override{Check(r==Pointer(0x600),"action object");Event("action:"+std::to_string(action));if(mutation==5)receivers&=~2u;}
        void* SecondaryCharacterForAnalysis() override{Event("secondary");return receivers&2?Pointer(0x700):nullptr;}
        void* CharacterField24ForAnalysis(void* c) override{Check(c==Pointer(0x700),"secondary token");Event("character24");return receivers&4?Pointer(0x800):nullptr;}
        void* MainReceiverForAnalysis() override{Event("mainReceiver");return receivers&8?Pointer(0x900):nullptr;}
        void SendDirectedNotificationForAnalysis(void* r,wxCharacterState& source,std::uint32_t code,const std::array<std::uint32_t,2>& payload) override
        {Check(&source==&state,"packet source");Event("notify:"+std::to_string(Token(r))+':'+std::to_string(code)+':'+std::to_string(payload[0])+':'+std::to_string(payload[1]));if(mutation==2){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(77));}}
        void SendFilteredNotificationForAnalysis(wxCharacterState& source,std::uint32_t code,std::uint32_t filter,const std::array<std::uint32_t,2>& payload) override
        {Check(&source==&state,"filtered source");Event("filter:"+std::to_string(code)+':'+std::to_string(filter)+':'+std::to_string(payload[0])+':'+std::to_string(payload[1]));}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));if(mutation==1){request.packedKey=0xf8000081;state.SetPendingHandleForAnalysis(Pointer(77));}return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return false;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool modeFlag,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(modeFlag)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==4)state.SetPendingHandleForAnalysis(Pointer(99));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float) override{Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"consuming query");Event("query:"+std::to_string(Token(h)));if(!h)return true;const bool matched=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return matched;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,
        unsigned flags,std::int32_t mode,unsigned allowed,unsigned receivers,unsigned mutation,unsigned tag,unsigned profile)
    {
        wxBlastState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.mode=mode;host.allowed=allowed;host.receivers=receivers;host.mutation=mutation;host.tag=tag;host.profile=profile;
        unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x34)result=state.vfunc_34(key);
        else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x3c)state.vfunc_3C(Pointer(0x300));
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();
        else throw std::runtime_error("slot");
        std::ostringstream o;o<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return o.str();
    }
    void Lifecycle()
    {
        wxBlastState state;Check(state.IsExactly(wxBlastState::ClassID)&&state.IsKindOf(wxCharacterState::ClassID)&&state.GetStateSelectorForAnalysis()==22,"RTTI/base/selector");
        Check(dynamic_cast<wxBlastState*>(spRTTIManager::Instance().Create(wxBlastState::ClassID).get()),"factory");
        state.SetPendingHandleForAnalysis(Pointer(11));state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone();auto* fresh=dynamic_cast<wxBlastState*>(clone.get());Check(fresh&&!fresh->GetPendingHandleForAnalysis()&&Flags(*fresh)==31,"clone fresh");
        wxBlastState target;target.SetPendingHandleForAnalysis(Pointer(77));spCloneManager manager;Check(state.vfunc_14(target,manager)&&target.GetPendingHandleForAnalysis()==Pointer(77),"Copy empty");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==15&&std::string(argv[1])=="--case")
        {std::int64_t v[13]{};for(unsigned i=0;i<13;++i)v[i]=std::stoll(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),std::int32_t(v[7]),unsigned(v[8]),unsigned(v[9]),unsigned(v[10]),unsigned(v[11]),unsigned(v[12]))<<'\n';return 0;}
        Lifecycle();wxBlastState unbound;Check(unbound.vfunc_34(0),"null permission before host");
        Check(Case(0x3c,0,11,22,0,0,31,4,1,15,0,1,0).find("action:8")!=std::string::npos,"blast substring and tier4");
        Check(Case(0x3c,0,11,22,0,0,31,4,0,15,0,0,0).find("notify:2304:10193:204:0")!=std::string::npos,"failure packet CC");
        Check(Case(0x1c,0,11,22,0,0,31,0,1,15,1,0,0).find("filter:10112:8:0:0")!=std::string::npos,"variant reread after lookup");
        Check(Case(0x3c,0,11,22,0,0,31,0,1,15,6,0,1).find("action:8")!=std::string::npos,"PS2 reload mode before threshold4");
        std::cout<<"PASS Blast lifecycle, notification/resource branches and entry\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
