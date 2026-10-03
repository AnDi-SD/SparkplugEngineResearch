#include "Code/wxDefendingState.h"
#include "Analysis/Host/wxDefendingStateHost.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool v,const char* m) { if(!v){std::cerr<<m<<'\n';std::exit(EXIT_FAILURE);} }
    void* Pointer(std::uintptr_t v) {return reinterpret_cast<void*>(v);}
    std::uintptr_t Token(void* v) {return reinterpret_cast<std::uintptr_t>(v);}
    float Float(unsigned v) {float f;std::memcpy(&f,&v,4);return f;}
    unsigned Bits(float f) {unsigned v;std::memcpy(&v,&f,4);return v;}
    unsigned Flags(const wxCharacterState& s) {unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v;}
    struct Host final : wxDefendingStateHost
    {
        wxDefendingState& state; wxAnimationRequestForAnalysis& request;
        bool ps2=false,predicate=false,masterPresent=true,endPresent=true,hitPresent=true,subPresent=true;
        unsigned next=22,first=11,second=11,word=0,hitByte=0,clock=100,delta=Bits(.1f),stage=0,motion=Bits(1),speed=0;
        std::array<unsigned,8> master{Bits(3),Bits(4),Bits(5),0,Bits(7),Bits(8),Bits(9),0xA4};
        std::array<unsigned,8> end=master,hit=master,sub{Bits(10),Bits(20),Bits(30),0,0,0,0,0};
        std::string trace;
        Host(wxDefendingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';auto f=state.GetFieldsForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)+':'+std::to_string(Flags(state))+':'+std::to_string(Bits(f.field48))+':'+std::to_string(f.field4C)+':'+std::to_string(f.field50)+':'+std::to_string(stage)+':'+std::to_string(hitByte);
        }
        std::array<unsigned,8>& Node(void* n)
        {if(n==Pointer(0x300))return master;if(n==Pointer(0x301))return end;if(n==Pointer(0x302))return hit;if(n==Pointer(0x303))return sub;throw std::logic_error("required node is absent");}
        wxDefendingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept override
        {return ps2?wxDefendingNumericProfileForAnalysis::PS2Finite:wxDefendingNumericProfileForAnalysis::PCNearest64;}
        void* OwnerNodeSearchRootForAnalysis(void*) override {return Pointer(0x600);}
        void* FindDefendingNodeForAnalysis(void* parent,const char* name,bool recursive,bool alternate) override
        {
            Require(recursive&&!alternate,"node lookup flags");const std::string s=name;Event("find:"+s);
            if(s=="shield_master"||s=="SubMaster")Require(parent==Pointer(0x600),"owner search root");else Require(parent==Pointer(0x300),"master search root");
            if(s=="shield_master")return masterPresent?Pointer(0x300):nullptr;
            if(s=="shield_end")return endPresent?Pointer(0x301):nullptr;
            if(s=="shield_hit")return hitPresent?Pointer(0x302):nullptr;
            if(s=="SubMaster")return subPresent?Pointer(0x303):nullptr;
            throw std::logic_error("unknown node name");
        }
        void InvokeDefendingNodeForAnalysis(void* n,bool e,bool p) override
        {Node(n);const std::string name=n==Pointer(0x300)?"master":n==Pointer(0x301)?"end":"hit";Event("node:"+name+':'+std::to_string(e)+':'+std::to_string(p));}
        unsigned ReadDefendingNodeWordForAnalysis(void* n,unsigned o) override {return Node(n)[(o-0x20)/4];}
        void WriteDefendingNodeWordForAnalysis(void* n,unsigned o,unsigned v) override {Node(n)[(o-0x20)/4]=v;}
        unsigned ReadDefendingNodeFlagsForAnalysis(void* n) override {return Node(n)[7];}
        void WriteDefendingNodeFlagsForAnalysis(void* n,unsigned f) override {Node(n)[7]=f;}
        unsigned OwnerDefendingPackedWordForAnalysis(void*) override {return word;}
        void SendDefendingMessageForAnalysis(void*,const wxCharacterState& s,unsigned code,unsigned a,unsigned b) override
        {Require(&s==&state,"message sender");Event("message:"+std::to_string(code)+':'+std::to_string(a)+':'+std::to_string(b));}
        void InvokeDefendingParticleForAnalysis(void* n,const char* name) override
        {Require(n==Pointer(0x300)&&std::string(name)=="ptc","particle lookup");Event("particle-find");Event("particle-call");}
        unsigned ReadSharedDefendingStageForAnalysis() override {return stage;}
        void WriteSharedDefendingStageForAnalysis(unsigned v) override {stage=v;}
        float ReadDefendingDeltaForAnalysis() override {return Float(delta);}
        unsigned ReadDefendingClockForAnalysis() override {return clock;}
        void SetConsumerSpeedForAnalysis(void*,float v) override {Event("speed:"+std::to_string(Bits(v)));speed=Bits(v);}
        void* DirectDefendingControlForAnalysis(void*) override {return Pointer(0x500);}
        std::uint8_t ReadDefendingHitByteForAnalysis(void* c) override {Require(c==Pointer(0x500),"captured direct control");return std::uint8_t(hitByte);}
        void WriteDefendingHitByteForAnalysis(void* c,std::uint8_t v) override {Require(c==Pointer(0x500),"captured hit control");Event("hit-byte:"+std::to_string(v));hitByte=v;}
        void InvokeDefendingScalarServiceForAnalysis(unsigned w,float a,float b,float c) override
        {Event("service:"+std::to_string(w)+':'+std::to_string(Bits(a))+':'+std::to_string(Bits(b))+':'+std::to_string(Bits(c)));}
        void* ResolveAnimationForAnalysis(void*,unsigned k) override {Event("lookup:"+std::to_string(k));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override {Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override {Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool m,unsigned f,bool i) override {Event("start:"+std::to_string(Token(h))+':'+std::to_string(m)+':'+std::to_string(f)+':'+std::to_string(i));}
        void StopAnimationForAnalysis(void*,void* h) override {Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float d) override {Require(d==.4f,"fade");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {Require(consume,"consume query");const auto t=unsigned(Token(h));Event("query:"+std::to_string(t));const bool done=!t||first==t||second==t;if(first==t)first=0;if(second==t)second=0;return done;}
        void ClearOwnerActionControlForAnalysis(void*) override {Event("word");motion=0;}
    };
    std::string Case(const std::uint64_t* v)
    {
        wxDefendingState state;wxAnimationRequestForAnalysis request{unsigned(v[2])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[3]));state.SetTransitionFlagsForAnalysis(v[8]&1,v[8]&2,v[8]&4,v[8]&8,v[8]&16);
        state.SetFieldsForAnalysis({v[9]?Pointer(0x300):nullptr,v[10]?Pointer(0x301):nullptr,v[11]?Pointer(0x302):nullptr,Float(unsigned(v[14])),unsigned(v[15]),std::uint8_t(v[16])});
        host.ps2=v[0]!=0;host.next=unsigned(v[4]);host.predicate=v[5]!=0;host.first=unsigned(v[6]);host.second=unsigned(v[7]);host.masterPresent=v[9]!=0;host.endPresent=v[10]!=0;host.hitPresent=v[11]!=0;host.subPresent=v[12]!=0;host.word=unsigned(v[13]);host.hitByte=unsigned(v[17]);host.clock=unsigned(v[18]);host.delta=unsigned(v[19]);host.stage=unsigned(v[20]);host.master[7]=unsigned(v[21]);host.end[7]=unsigned(v[22]);host.motion=unsigned(v[23]);host.speed=unsigned(v[25]);host.sub[0]=unsigned(v[26]);host.sub[1]=unsigned(v[27]);host.sub[2]=unsigned(v[28]);
        unsigned result=2;
        if(v[1]==0x1C)result=state.vfunc_1C(request);else if(v[1]==0x20)result=state.vfunc_20(request);else if(v[1]==0x30)state.vfunc_30(request);
        else if(v[1]==0x104)state.InitializeNodesForAnalysis();else if(v[1]==0x108)state.SetNodesEnabledForAnalysis(v[2]!=0,v[5]!=0);else if(v[1]==0x10C)state.AdvanceExitNodesForAnalysis();else throw std::logic_error("Defending slot");
        const auto f=state.GetFieldsForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Flags(state)<<' '<<host.first<<' '<<host.second<<' '<<(f.field3C!=nullptr)<<' '<<(f.field40!=nullptr)<<' '<<(f.field44!=nullptr)<<' '<<Bits(f.field48)<<' '<<f.field4C<<' '<<unsigned(f.field50)<<' '<<host.motion<<' '<<host.hitByte<<' '<<host.stage<<' '<<host.speed;
        for(unsigned i:{0u,1u,2u,7u})out<<' '<<host.master[i];
        for(unsigned i:{4u,5u,6u,7u})out<<' '<<host.end[i];
        out<<' '<<host.hit[7]<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==31&&std::string(argv[1])=="--case"){std::uint64_t v[29]{};for(unsigned i=0;i<29;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxDefendingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(state.GetStateSelectorForAnalysis()==8&&!state.GetFieldsForAnalysis().field3C,"constructor");Require(dynamic_cast<wxDefendingState*>(spRTTIManager::Instance().Create(wxDefendingState::ClassID).get()),"factory");
    state.InitializeNodesForAnalysis();Require(state.GetFieldsForAnalysis().field44==Pointer(0x302),"node initialization");
    Require(!state.vfunc_1C(request)&&host.speed==Bits(2)&&!(Flags(state)&1),"first entry");
    host.first=host.second=22;Require(state.vfunc_1C(request)&&host.speed==Bits(1),"completed entry");
    auto f=state.GetFieldsForAnalysis();f.field48=.17f;f.field4C=71;f.field50=7;state.SetFieldsForAnalysis(f);state.vfunc_40_ResetForAnalysis();
    Require(state.GetFieldsForAnalysis().field50==7&&state.GetFieldsForAnalysis().field4C==71,"base reset preserves own fields");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxDefendingState*>(clone.get());Require(fresh&&!fresh->GetFieldsForAnalysis().field3C&&!fresh->GetFieldsForAnalysis().field50,"fresh clone");
    wxDefendingState target;target.SetFieldsForAnalysis(f);spCloneManager manager;Require(state.vfunc_14(target,manager)&&target.GetFieldsForAnalysis().field4C==71,"empty copy");
    state.SetFieldsForAnalysis(f);host.hitByte=1;host.clock=0xFFFFFF80;state.vfunc_30(request);Require(state.GetFieldsForAnalysis().field4C==172&&!host.hitByte,"hit deadline wraps");
    std::cout<<"Defending checks passed\n";
}
