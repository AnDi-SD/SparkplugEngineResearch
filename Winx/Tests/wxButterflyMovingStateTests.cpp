#include "Code/wxButterflyMovingState.h"
#include "Analysis/Host/wxButterflyMovingStateHost.h"
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
    void Require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(EXIT_FAILURE);}}
    float Float(unsigned bits){float value;std::memcpy(&value,&bits,4);return value;}
    unsigned Bits(float value){unsigned bits;std::memcpy(&bits,&value,4);return bits;}
    void* Pointer(unsigned value){return reinterpret_cast<void*>(std::uintptr_t(value));}
    unsigned Token(void* value){return unsigned(reinterpret_cast<std::uintptr_t>(value));}
    unsigned Flags(const wxCharacterState& s){unsigned value=0;const auto f=s.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(f[i])value|=1u<<i;return value;}
    struct Host final:wxButterflyMovingStateHost
    {
        wxButterflyMovingState& state;wxAnimationRequestForAnalysis& request;
        std::array<unsigned,3> position{},scale{},random{},point{};
        std::array<unsigned,9> basis{Bits(1),0,0,0,Bits(1),0,0,0,Bits(1)};
        unsigned nodeFlags=0xA4,clock=1,delta=0,next=22,first=0,second=0,rngCalls=0;bool predicate=false;
        std::string trace;
        Host(wxButterflyMovingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';const auto f=state.GetFieldsForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)+':'+std::to_string(Flags(state))+':'+std::to_string(f.initialized)+':'+std::to_string(f.deadline)+':'+std::to_string(unsigned(f.phase))+':'+std::to_string(Bits(f.desiredHeight));
        }
        void* ButterflyNodeForAnalysis(void*) override{return Pointer(0x300);}
        float ReadButterflyNodeWordForAnalysis(void* n,unsigned offset) override
        {Require(n==Pointer(0x300),"node");if(offset<0x30)return Float(position[(offset-0x20)/4]);if(offset<0x40)return Float(scale[(offset-0x30)/4]);return Float(basis[(offset-0x40)/4]);}
        void WriteButterflyNodeWordForAnalysis(void* n,unsigned offset,float value) override
        {
            Require(n==Pointer(0x300),"node");Event("write:"+std::to_string(offset)+':'+std::to_string(Bits(value)));
            if(offset<0x30)position[(offset-0x20)/4]=Bits(value);else if(offset<0x40)scale[(offset-0x30)/4]=Bits(value);else basis[(offset-0x40)/4]=Bits(value);
        }
        void MarkButterflyNodeDirtyForAnalysis(void* n) override{Require(n==Pointer(0x300),"node");Event("write:176:"+std::to_string(nodeFlags|1));nodeFlags|=1;}
        void InvokeButterflyNodeForAnalysis(void* n,bool argument) override{Require(n==Pointer(0x300)&&!argument,"node call");Event("node-world");}
        unsigned ButterflyRandomForAnalysis() override{Event("rng");Require(rngCalls<3,"RNG fixture exhausted");return random[rngCalls++];}
        void ButterflyRandomPointForAnalysis(Vector3& output,float radius,Vector3 origin) override
        {Require(radius==200&&origin==state.GetFieldsForAnalysis().origin,"random point arguments");Event("random-point");for(unsigned i=0;i<3;++i)output[i]=Float(point[i]);}
        unsigned ButterflyClockForAnalysis() override{return clock;}
        float ButterflyDeltaForAnalysis() override{return Float(delta);}
        void* ResolveAnimationForAnalysis(void*,unsigned key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,unsigned fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float duration) override{Require(duration==.4f,"fade");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Require(consume,"consume");Event("query:"+std::to_string(Token(h)));return first==Token(h)||second==Token(h);}
        void ClearOwnerActionControlForAnalysis(void*) override{throw std::logic_error("unexpected control write");}
    };
    std::string Case(const unsigned* v)
    {
        wxButterflyMovingState state;wxAnimationRequestForAnalysis request{v[1]};Host h(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&h);state.SetPendingHandleForAnalysis(Pointer(v[2]));state.SetTransitionFlagsForAnalysis(v[5]&1,v[5]&2,v[5]&4,v[5]&8,v[5]&16);
        auto f=state.GetFieldsForAnalysis();f.initialized=v[6];f.deadline=v[7];f.phase=std::uint8_t(v[8]);f.desiredHeight=Float(v[17]);
        for(unsigned i=0;i<3;++i){f.origin[i]=Float(v[11+i]);f.target[i]=Float(v[14+i]);f.current[i]=Float(v[18+i]);h.position[i]=v[21+i];h.random[i]=v[34+i];h.point[i]=v[37+i];h.scale[i]=v[40+i];}
        for(unsigned i=0;i<9;++i)h.basis[i]=v[24+i];state.SetFieldsForAnalysis(f);h.next=v[3];h.predicate=v[4]!=0;h.clock=v[9];h.delta=v[10];h.nodeFlags=v[33];h.first=v[44];h.second=v[45];
        unsigned result=2;if(v[0]==0x1C)result=state.vfunc_1C(request);else if(v[0]==0x20)result=state.vfunc_20(request);else if(v[0]==0x30)state.vfunc_30(request);else if(v[0]==0x100)state.AdvancePhaseForAnalysis();else if(v[0]==0x104)state.MoveForAnalysis();else throw std::logic_error("Butterfly slot");
        f=state.GetFieldsForAnalysis();std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Flags(state);
        for(float x:f.origin)out<<' '<<Bits(x);for(float x:f.target)out<<' '<<Bits(x);out<<' '<<Bits(f.desiredHeight);for(float x:f.current)out<<' '<<Bits(x);out<<' '<<f.initialized<<' '<<f.deadline<<' '<<unsigned(f.phase);
        for(auto x:h.position)out<<' '<<x;for(auto x:h.scale)out<<' '<<x;for(auto x:h.basis)out<<' '<<x;out<<' '<<h.nodeFlags<<' '<<h.first<<' '<<h.second<<' '<<h.rngCalls<<'|'<<h.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==48&&std::string(argv[1])=="--case"){unsigned v[46]{};for(unsigned i=0;i<46;++i)v[i]=unsigned(std::stoull(argv[i+2]));std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxButterflyMovingState s;Require(s.GetStateSelectorForAnalysis()==0&&s.GetFieldsForAnalysis().phase==1&&!s.GetFieldsForAnalysis().initialized,"constructor");
    Require(!s.vfunc_34(34)&&!s.vfunc_38(1),"permission");Require(dynamic_cast<wxButterflyMovingState*>(spRTTIManager::Instance().Create(wxButterflyMovingState::ClassID).get()),"factory");
    auto f=s.GetFieldsForAnalysis();f.initialized=7;f.phase=255;f.deadline=19;f.origin={1,2,3};s.SetFieldsForAnalysis(f);s.vfunc_40_ResetForAnalysis();Require(s.GetFieldsForAnalysis().initialized==7&&s.GetFieldsForAnalysis().phase==255,"reset preserves own fields");
    auto cloned=s.Clone();auto* c=dynamic_cast<wxButterflyMovingState*>(cloned.get());Require(c&&c->GetFieldsForAnalysis().phase==1&&!c->GetFieldsForAnalysis().initialized,"fresh clone");
    wxButterflyMovingState target;target.SetFieldsForAnalysis(f);spCloneManager manager;Require(s.vfunc_14(target,manager)&&target.GetFieldsForAnalysis().deadline==19,"copy preserves target fields");
    wxAnimationRequestForAnalysis request;bool missing=false;try{s.vfunc_30(request);}catch(const std::logic_error&){missing=true;}Require(missing,"missing host fails");
    Host host(s,request);s.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    f={};f.phase=1;f.origin[1]=-0x1p-50f;s.SetFieldsForAnalysis(f);host.random={1024,0,0};host.position={Bits(1),Bits(2),Bits(3)};host.point={Bits(1000),0,0};s.vfunc_30(request);
    Require(Bits(s.GetFieldsForAnalysis().desiredHeight)==0xC1EFFFF9u,"native height midpoint retains small negative origin");
    f={};f.phase=1;f.origin[1]=-0x1p-60f;s.SetFieldsForAnalysis(f);host.rngCalls=0;s.vfunc_30(request);
    Require(Bits(s.GetFieldsForAnalysis().desiredHeight)==0xC1EFFFF8u,"native nearest64 half tie preserves even midpoint");
    std::cout<<"Butterfly lifecycle checks passed\n";
}
