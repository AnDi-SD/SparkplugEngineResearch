#include "Code/wxSpiderHurtState.h"
#include "Analysis/Host/wxSpiderHurtStateHost.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
    void* Pointer(std::uintptr_t x){return reinterpret_cast<void*>(x);}
    std::uintptr_t Token(void* x){return reinterpret_cast<std::uintptr_t>(x);}
    struct Host final : wxSpiderHurtStateHost
    {
        wxSpiderHurtState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;bool predicate=false,gate=true;
        std::uint32_t clock=100,control=0xffffffff,rate=0;std::string trace;
        Host(wxSpiderHurtState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        unsigned Flags() const
        { unsigned n=0;auto flags=state.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(flags[i])n|=1u<<i;return n; }
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';auto r=state.GetRuntimeForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags())
                +':'+std::to_string(request.packedKey)+':'+std::to_string(r.flag3C)+':'+std::to_string(r.permission4C)+':'+std::to_string(r.deadline48);
        }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override
        {Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float duration) override{Check(duration==0.4f,"fade");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {
            Check(consume,"consuming query");auto token=Token(h);Event("query:"+std::to_string(token));
            if(!token)return true;bool done=first==token||second==token;if(first==token)first=0;if(second==token)second=0;return done;
        }
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");control=0;}
        void SetConsumerRateForAnalysis(void*,float value) override{Check(value==1.0f,"rate");Event("rate");rate=0x3f800000;}
        std::uint32_t ClockWordForAnalysis() override{Event("clock");return clock;}
        void* OwnerPermissionGateForAnalysis(void*) override{Event("gate");return gate?Pointer(1):nullptr;}
    };
    std::string Case(const std::uint64_t* v)
    {
        wxSpiderHurtState state;wxAnimationRequestForAnalysis request{std::uint32_t(v[1])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(100),Pointer(200),&host);state.SetPendingHandleForAnalysis(Pointer(v[2]));
        state.SetTransitionFlagsForAnalysis(v[5]&1,v[5]&2,v[5]&4,v[5]&8,v[5]&16);
        state.SetRuntimeForAnalysis({v[10]!=0,v[11]!=0,std::uint32_t(v[12]),std::uint32_t(v[13]),std::uint32_t(v[14]),v[15]!=0});
        host.next=v[3];host.predicate=v[4]!=0;host.first=v[6];host.second=v[7];host.clock=std::uint32_t(v[8]);host.gate=v[9]!=0;unsigned result=2;
        switch(v[0])
        {
        case 0x1c:result=state.vfunc_1C(request);break;case 0x20:result=state.vfunc_20(request);break;
        case 0x24:state.vfunc_24();break;case 0x28:state.vfunc_28(request);break;case 0x2c:state.vfunc_2C(request);break;
        case 0x30:state.vfunc_30(request);break;case 0x34:result=state.vfunc_34(request.packedKey);break;
        case 0x38:result=state.vfunc_38(request.packedKey);break;case 0x3c:state.vfunc_3C(nullptr);break;
        case 0x40:state.vfunc_40_ResetForAnalysis();break;default:throw std::logic_error("slot");
        }
        auto r=state.GetRuntimeForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.control
            <<' '<<host.first<<' '<<host.second<<' '<<host.Flags()<<' '<<r.flag3C<<' '<<r.flag3D<<' '<<r.word40
            <<' '<<r.delay44<<' '<<r.deadline48<<' '<<r.permission4C<<' '<<host.rate<<'|'<<host.trace;return out.str();
    }
    void Lifetime()
    {
        wxSpiderHurtState state;auto r=state.GetRuntimeForAnalysis();
        Check(state.GetStateSelectorForAnalysis()==10&&!r.flag3C&&!r.flag3D&&!r.word40&&r.delay44==10000&&!r.deadline48&&r.permission4C,"ctor runtime");
        state.SetRuntimeForAnalysis({true,true,1,2,3,false});state.SetPendingHandleForAnalysis(Pointer(11));
        state.SetTransitionFlagsForAnalysis(false,false,false,false,false);spCloneManager manager;auto clone=state.vfunc_10(manager);
        auto* fresh=dynamic_cast<wxSpiderHurtState*>(clone.get());Check(fresh&&manager.FindClone(state)==fresh,"clone registered");r=fresh->GetRuntimeForAnalysis();
        Check(!r.flag3C&&!r.flag3D&&!r.word40&&r.delay44==10000&&!r.deadline48&&r.permission4C&&!fresh->GetPendingHandleForAnalysis()
            &&fresh->GetTransitionFlagsForAnalysis()==std::array<bool,5>{true,true,true,true,true},"fresh clone defaults");
        Check(fresh->IsExactly(state.ClassID)&&fresh->IsKindOf(wxCharacterState::ClassID)&&bool(spRTTIManager::Instance().Create(state.ClassID)),"RTTI/factory");
        state.vfunc_40_ResetForAnalysis();r=state.GetRuntimeForAnalysis();Check(r.flag3C&&r.flag3D&&r.word40==1&&r.delay44==2&&r.deadline48==3&&!r.permission4C,"base reset preserves owned fields");
        wxAnimationRequestForAnalysis request;bool rejected=false;try{state.vfunc_1C(request);}catch(const std::logic_error&){rejected=true;}Check(rejected,"missing host");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==18&&std::string(argv[1])=="--case")
        {std::uint64_t v[16];for(unsigned i=0;i<16;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return 0;}
        Lifetime();std::uint64_t v[16]{0x1c,0xffffffff,11,22,0,31,11,11,100,1,1,1,73,10000,100,1};
        Check(Case(v).find("stop")==std::string::npos,"first entry keeps previous pending");
        v[0]=0x30;v[5]=0;Check(Case(v).find("clock")!=std::string::npos,"update reads clock after one-shot animation");
        v[0]=0x34;v[9]=0;Check(Case(v).rfind("gate")!=std::string::npos,"permission reads owner graph");
        std::cout<<"PASS SpiderHurt lifecycle and operations\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
