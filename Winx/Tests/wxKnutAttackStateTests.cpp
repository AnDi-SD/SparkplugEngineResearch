#include "Code/wxKnutAttackState.h"
#include "Analysis/Host/wxKnutAttackStateHost.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value,const char* message)
    { if(!value){std::cerr<<message<<'\n';std::exit(EXIT_FAILURE);} }
    void* Pointer(std::uintptr_t x){return reinterpret_cast<void*>(x);}
    std::uintptr_t Token(void* p){return reinterpret_cast<std::uintptr_t>(p);}
    unsigned Flags(const wxCharacterState& state)
    { unsigned flags=0;const auto v=state.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(v[i])flags|=1u<<i;return flags; }
    const std::vector<std::string>& Names()
    {
        static const std::vector<std::string> result=[]
        {
            std::vector<std::string> tags={"event_turnleft_begin","event_turnleft_end","event_turnright_begin","event_turnright_end","event_turn_begin","event_turn_end","event_defense_begin","event_defense_end","event_charge_begin","event_hurt_begin","event_hurt_end","begin","scepter_start","scepter_end"};
            std::vector<std::string> all=tags;
            for(const auto& t:tags)all.push_back("prefix_"+t);
            for(const auto& t:tags)all.push_back(t+"_suffix");
            for(const auto& t:tags)all.push_back(t.substr(0,t.size()-1));
            for(auto t:tags){t[0]='E';all.push_back(t);}
            all.insert(all.end(),{"","event_land","scepter_start_scepter_end","begin_scepter_start","event_hurt_begin_suffix"});return all;
        }();return result;
    }
    struct Host final : wxKnutAttackStateHost
    {
        wxCharacterState& state;wxAnimationRequestForAnalysis& request;
        unsigned motionFlag=0,word=0;std::uintptr_t first=0,second=0,next=22;bool predicate=false;
        std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& event)
        { if(!trace.empty())trace+=';';trace+=event+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey); }
        std::uint8_t OwnerEntityMotionFlag1FForAnalysis(void*) override {Event("motionflag");return std::uint8_t(motionFlag);}
        const char* EventTagNameForAnalysis(const void*) override {Event("tag");return Names().at(first).c_str();}
        void* OwnerField24ForAnalysis(void*) override {Event("owner24");return second&1?Pointer(0x400):nullptr;}
        void* GameCoreField2B4ForAnalysis() override {Event("core2B4");return second&2?Pointer(0x600):nullptr;}
        void* OwnerEntityControllerForAnalysis(void*) override {Event("controller");return Pointer(0x500);}
        void InvokeControllerSlot38ForAnalysis(void* controller,unsigned arg) override {Require(controller==Pointer(0x500),"borrowed controller");Event("slot38:"+std::to_string(arg));}
        void InvokeControllerSlot3CForAnalysis(void* controller,unsigned a,unsigned b) override {Require(controller==Pointer(0x500),"borrowed controller");Event("slot3C:"+std::to_string(a)+':'+std::to_string(b));}
        void SendNotificationForAnalysis(void* receiver,wxCharacterState& source,unsigned code,wxKnutNotificationWordForAnalysis a,wxKnutNotificationWordForAnalysis b) override
        {Require(&source==&state && (receiver==Pointer(0x400) || receiver==Pointer(0x600)),"borrowed notification");Event("packet:"+std::to_string(Token(receiver))+':'+std::to_string(code)+':'+std::to_string(a.value&a.knownMask)+':'+std::to_string(a.knownMask)+':'+std::to_string(b.value&b.knownMask)+':'+std::to_string(b.knownMask));}
        void* ResolveAnimationForAnalysis(void*,unsigned key) override {Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override {Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* handle) override {Event("reset:"+std::to_string(Token(handle)));if(first==Token(handle))first=0;if(second==Token(handle))second=0;}
        void StartAnimationForAnalysis(void*,void* handle,bool mode,unsigned fade,bool interrupt) override {Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* handle) override {Event("stop:"+std::to_string(Token(handle)));}
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override {Require(duration==0.4f,"fade");Event("fade:"+std::to_string(Token(handle)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {Require(consume,"consuming query");const auto token=Token(handle);Event("query:"+std::to_string(token));if(!token)return true;const bool done=first==token || second==token;if(first==token)first=0;if(second==token)second=0;return done;}
        void ClearOwnerActionControlForAnalysis(void*) override {Event("word");word=0;}
    };
    std::string Case(const std::uint64_t* v)
    {
        wxKnutAttackState state;wxAnimationRequestForAnalysis request{std::uint32_t(v[1])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[2]));
        state.SetTransitionFlagsForAnalysis(v[7]&1,v[7]&2,v[7]&4,v[7]&8,v[7]&16);
        host.next=v[3];host.predicate=v[4]!=0;host.first=v[5];host.second=v[6];host.word=unsigned(v[8]);host.motionFlag=unsigned(v[9]);
        unsigned result=2;
        switch(v[0])
        {
            case 0x1C:result=state.vfunc_1C(request);break;
            case 0x20:result=state.vfunc_20(request);break;
            case 0x30:state.vfunc_30(request);break;
            case 0x34:result=state.vfunc_34(request.packedKey);break;
            case 0x3C:state.vfunc_3C(Pointer(0x300));break;
            default:throw std::logic_error("Knut case slot");
        }
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==12 && std::string(argv[1])=="--case")
    {std::uint64_t v[10]{};for(unsigned i=0;i<10;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxKnutAttackState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);
    Require(state.GetStateSelectorForAnalysis()==3 && state.IsExactly(wxKnutAttackState::ClassID) && state.IsKindOf(wxCharacterState::ClassID),"identity");
    Require(dynamic_cast<wxKnutAttackState*>(spRTTIManager::Instance().Create(wxKnutAttackState::ClassID).get()),"factory");
    state.SetPendingHandleForAnalysis(Pointer(11));host.motionFlag=1;state.vfunc_30(request);
    Require(request.packedKey==0xF0987F8F && state.GetPendingHandleForAnalysis()==Pointer(22) && !(Flags(state)&2),"first update selects variant and clears once after playback");
    host.trace.clear();request.packedKey=0xFFFFFFFF;state.vfunc_30(request);Require(host.trace.empty() && request.packedKey==0xFFFFFFFF,"subsequent update leaves key and services untouched");
    host.first=9;host.second=3;state.vfunc_3C(Pointer(3));Require(host.trace.find("packet:1024:10054:1:255:0:4294967295")!=std::string::npos && host.trace.find("packet:1536:10060:1:255:3:4294967295")!=std::string::npos,"hurt two receivers");
    host.trace.clear();host.first=14+9;state.vfunc_3C(Pointer(3));Require(host.trace.find("slot38:1")!=std::string::npos && host.trace.find("core2B4")==std::string::npos,"exact hurt precedes generic begin substring");
    host.trace.clear();host.first=5;state.vfunc_3C(Pointer(3));Require(host.trace.find("slot38")==std::string::npos,"exact end event");
    state.vfunc_40_ResetForAnalysis();host.trace.clear();state.vfunc_30(request);Require(!(Flags(state)&2) && host.trace.find("motionflag")!=std::string::npos,"Reset rearms once update");
    host.first=host.second=22;Require(state.vfunc_34(0) && !host.first && !host.second,"consume both records");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxKnutAttackState*>(clone.get());Require(fresh && Flags(*fresh)==31 && !fresh->GetPendingHandleForAnalysis(),"fresh clone");
    wxKnutAttackState target;target.SetPendingHandleForAnalysis(Pointer(33));spCloneManager manager;Require(state.vfunc_14(target,manager) && target.GetPendingHandleForAnalysis()==Pointer(33),"empty inherited Copy");
    std::cout<<"KnutAttack state checks passed\n";
}
