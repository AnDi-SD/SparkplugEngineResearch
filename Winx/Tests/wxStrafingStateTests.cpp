#include "Code/wxStrafingState.h"
#include "Analysis/Host/wxStrafingStateHost.h"
#include "Analysis/PC/spNodeTransformMath.h"
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
    void Require(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(EXIT_FAILURE);}}
    void* Pointer(std::uintptr_t v){return reinterpret_cast<void*>(v);}
    std::uintptr_t Token(void* v){return reinterpret_cast<std::uintptr_t>(v);}
    float Float(unsigned v){float f;std::memcpy(&f,&v,4);return f;}
    unsigned Bits(float f){unsigned v;std::memcpy(&v,&f,4);return v;}
    unsigned Flags(const wxCharacterState& s){unsigned v=0;auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])v|=1u<<i;return v;}
    struct Host final : wxStrafingStateHost
    {
        wxStrafingState& state;wxAnimationRequestForAnalysis& request;
        bool ps2=false,ps2AxisFixture=false,predicate=false,cached=true,targetPresent=true,field148=false;
        unsigned next=22,first=11,second=11,binding=1,motion=Bits(1),direct=Bits(1),word3C=7,word44=7,byte64=7,speed=0;
        unsigned angle8=Bits(.1f),angle14=Bits(1),normalized=Bits(1),rightPair=Bits(1),forwardPair=Bits(1),angleIndex=0;
        std::string trace;
        Host(wxStrafingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& name)
        {if(!trace.empty())trace+=';';trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)+':'+std::to_string(Flags(state))+':'+std::to_string(state.GetField3CForAnalysis()!=nullptr)+':'+std::to_string(binding)+':'+std::to_string(word3C)+':'+std::to_string(word44)+':'+std::to_string(byte64);}
        wxStrafingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept override{return ps2?wxStrafingNumericProfileForAnalysis::PS2Finite:wxStrafingNumericProfileForAnalysis::PC;}
        void* ReadCachedStrafingObserverForAnalysis(void*) override{Event("cached");return cached?Pointer(0x300):nullptr;}
        void* ReadObserverBindingForAnalysis(void* o) override{Require(o==Pointer(0x300),"observer");return Pointer(binding);}
        void WriteObserverBindingForAnalysis(void* o,void* b) override{Require(o==Pointer(0x300),"observer binding");Event("binding:"+std::to_string(Token(b)));binding=unsigned(Token(b));}
        void* ReadStrafingEntityControlForAnalysis(void*) override{return targetPresent?Pointer(2):nullptr;}
        void RemoveStrafingObserverForAnalysis(void* b,void* o) override{Require(o==Pointer(0x300),"remove observer");Event("remove:"+std::to_string(Token(b)));}
        void AddStrafingObserverForAnalysis(void* b,void* o) override{Require(o==Pointer(0x300),"add observer");Event("add:"+std::to_string(Token(b)));}
        void WriteStrafingObserverWordForAnalysis(void* o,unsigned off,unsigned v) override{Require(o==Pointer(0x300),"observer word");Event("observer-word:"+std::to_string(off)+':'+std::to_string(v));(off==0x3C?word3C:word44)=v;}
        void WriteStrafingObserverByte64ForAnalysis(void* o,std::uint8_t v) override{Require(o==Pointer(0x300),"observer byte");Event("observer-byte:"+std::to_string(v));byte64=v;}
        bool HasStrafingEntityField148ForAnalysis(void*) override{return field148;}
        void* ReadStrafingMotionObjectForAnalysis(void*) override{return Pointer(0x500);}
        float ReadStrafingMotionWordForAnalysis(void* o,unsigned off) override{Require(o==Pointer(0x500),"motion source");return Float(off==4?motion:off==8?angle8:angle14);}
        void SetConsumerSpeedForAnalysis(void*,float v) override{Event("speed:"+std::to_string(Bits(v)));speed=Bits(v);}
        void WriteStrafingDirectMotionForAnalysis(void*,float v) override{Event("word:"+std::to_string(Bits(v)));direct=Bits(v);}
        void SendStrafingMessageForAnalysis(void*,const wxCharacterState& s,unsigned code,bool enabled,unsigned word) override{Require(&s==&state,"sender");Event("message:"+std::to_string(code)+':'+std::to_string(enabled)+':'+std::to_string(word));}
        void NormalizeStrafingAngleForAnalysis(float& a) override{Event("angle-normalize:"+std::to_string(Bits(a)));a=Float(normalized);}
        Vector3 ReadStrafingBasisForAnalysis(void*,unsigned row) override{return row==0?Vector3{1,5,0}:Vector3{0,7,1};}
        void NormalizeStrafingBasisForAnalysis(Vector3& a) override
        {Event("basis-normalize:"+std::to_string(Bits(a[0]))+':'+std::to_string(Bits(a[1]))+':'+std::to_string(Bits(a[2])));if(ps2){if(!ps2AxisFixture||(a!=Vector3{1,0,0}&&a!=Vector3{0,0,1}))throw std::logic_error("PS2 EE normalization service not supplied for this input");return;}sparkplug::evidence::pc::node_math::Normalize(a);}
        float InvokeStrafingVectorAngleForAnalysis(const Vector3& a) override
        {Event("vector-angle:"+std::to_string(Bits(a[0]))+':'+std::to_string(Bits(a[1]))+':'+std::to_string(Bits(a[2])));return angleIndex++==0?.3f:.7f;}
        float InvokeStrafingAnglePairForAnalysis(float a,float b) override
        {Event("angle-pair:"+std::to_string(Bits(a))+':'+std::to_string(Bits(b)));return Float(angleIndex==1?rightPair:forwardPair);}
        void* ResolveAnimationForAnalysis(void*,unsigned k) override{Event("lookup:"+std::to_string(k));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool m,unsigned f,bool i) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(m)+':'+std::to_string(f)+':'+std::to_string(i));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float d) override{Require(d==.4f,"fade");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Require(consume,"query");const auto t=unsigned(Token(h));Event("query:"+std::to_string(t));const bool done=!t||first==t||second==t;if(first==t)first=0;if(second==t)second=0;return done;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word:0");direct=0;}
    };
    std::string Case(const std::uint64_t* v)
    {
        wxStrafingState state;wxAnimationRequestForAnalysis request{unsigned(v[2])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[3]));state.SetTransitionFlagsForAnalysis(v[8]&1,v[8]&2,v[8]&4,v[8]&8,v[8]&16);state.SetField3CForAnalysis(v[9]?Pointer(0x300):nullptr);
        host.ps2=v[0]!=0;host.ps2AxisFixture=host.ps2;host.next=unsigned(v[4]);host.predicate=v[5]!=0;host.first=unsigned(v[6]);host.second=unsigned(v[7]);host.cached=v[10]!=0;host.binding=unsigned(v[11]);host.targetPresent=v[12]!=0;host.motion=unsigned(v[13]);host.field148=v[14]!=0;host.angle8=unsigned(v[15]);host.angle14=unsigned(v[16]);host.normalized=unsigned(v[17]);host.rightPair=unsigned(v[18]);host.forwardPair=unsigned(v[19]);
        unsigned result=2;if(v[1]==0x1C)result=state.vfunc_1C(request);else if(v[1]==0x20)result=state.vfunc_20(request);else if(v[1]==0x30)state.vfunc_30(request);else throw std::logic_error("Strafing slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Flags(state)<<' '<<host.first<<' '<<host.second<<' '<<host.direct<<' '<<(state.GetField3CForAnalysis()!=nullptr)<<' '<<host.binding<<' '<<host.word3C<<' '<<host.word44<<' '<<host.byte64<<' '<<host.speed<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==23&&std::string(argv[1])=="--case"){std::uint64_t v[21]{};for(unsigned i=0;i<21;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxStrafingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(!state.GetStateSelectorForAnalysis()&&!state.GetField3CForAnalysis(),"constructor");Require(dynamic_cast<wxStrafingState*>(spRTTIManager::Instance().Create(wxStrafingState::ClassID).get()),"factory");
    host.motion=0;Require(!state.vfunc_1C(request)&&host.binding==2&&host.word3C==0x7F800000&&host.word44==2&&!host.byte64&&host.direct==Bits(.01f),"entry observer and transition");
    state.SetTransitionFlagsForAnalysis(false,false,false,true,true);host.first=host.second=0;Require(!state.vfunc_20(request)&&!state.GetField3CForAnalysis()&&!host.binding,"repeat exit unregisters before query");
    state.SetField3CForAnalysis(Pointer(0x300));state.vfunc_40_ResetForAnalysis();Require(state.GetField3CForAnalysis()==Pointer(0x300),"base reset preserves observer");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxStrafingState*>(clone.get());Require(fresh&&!fresh->GetField3CForAnalysis(),"clone defaults");
    wxStrafingState target;target.SetField3CForAnalysis(Pointer(0x400));spCloneManager manager;Require(state.vfunc_14(target,manager)&&target.GetField3CForAnalysis()==Pointer(0x400),"empty copy");
    host.ps2=true;bool rejected=false;try{state.vfunc_30(request);}catch(const std::logic_error&){rejected=true;}Require(rejected,"missing PS2 EE service rejects");
    std::cout<<"Strafing checks passed\n";
}
