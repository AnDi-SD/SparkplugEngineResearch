#include "Code/wxVineClimbingState.h"
#include "Analysis/Host/wxVineClimbingStateHost.h"
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
    void Require(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    float Float(unsigned value) { float f; std::memcpy(&f, &value, 4); return f; }
    unsigned Bits(float value) { unsigned bits; std::memcpy(&bits, &value, 4); return bits; }
    unsigned Flags(const wxCharacterState& s) { unsigned v=0; auto f=s.GetTransitionFlagsForAnalysis(); for (unsigned i=0;i<5;++i) if(f[i]) v|=1u<<i; return v; }
    struct Host final : wxVineClimbingStateHost
    {
        wxVineClimbingState& state; wxAnimationRequestForAnalysis& request;
        bool ps2=false,predicate=false,found=false;
        unsigned next=22,first=11,second=11,motion=0x3F800000,angleA=0x3F800000,angleB=0x3F800000,byte50=0,heightA=0,heightB=0,entityFlag=7;
        Vector3 tracker{3,4,5},target{10,20,30}; unsigned trackerDirty=0xA4,targetDirty=0xA4;
        std::string trace;
        Host(wxVineClimbingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r) {}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';auto m=state.GetMovementFieldsForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)+':'+std::to_string(Flags(state))+':'+std::to_string(entityFlag)+':'+std::to_string(m.field2C)+':'+std::to_string(m.field28!=nullptr);
        }
        std::array<float,2> OwnerAnglesForAnalysis(void*) override { return {Float(angleA),Float(angleB)}; }
        float OwnerMotionForAnalysis(void*) override { return Float(motion); }
        std::uint8_t OwnerControlByte50ForAnalysis(void*) override { return std::uint8_t(byte50); }
        void* OwnerEntityControlForAnalysis(void*) override { return Pointer(0x700); }
        void WriteEntityControlFlagForAnalysis(void* c,std::uint8_t v) override { Require(c==Pointer(0x700),"entity control");Event("entity-flag:"+std::to_string(v));entityFlag=v; }
        std::array<float,2> ExitHeightsForAnalysis(void*) override { return {Float(heightA),Float(heightB)}; }
        void PrepareTurnForAnalysis(wxCharacterState& s) override { Require(&s==&state,"turn state");Event("turn"); }
        void ResetEntityControlForAnalysis(void* c) override { Require(c==Pointer(0x700),"reset control");Event("entity-reset"); }
        void RotateClimbingNodeForAnalysis(void* n,float angle) override { Require(n==Pointer(0x600),"rotation node");Event("rotate:"+std::to_string(Bits(angle))); }
        Vector3 ReadExitNodeAxisForAnalysis(void*) override { return {0,0,1}; }
        void TranslateExitNodeForAnalysis(void* n,const Vector3& delta) override
        { Require(n==Pointer(0x600),"translation node");Event("translate:"+std::to_string(Bits(delta[0]))+':'+std::to_string(Bits(delta[1]))+':'+std::to_string(Bits(delta[2])));for(unsigned i=0;i<3;++i)target[i]=float(double(target[i])+double(delta[i]));targetDirty|=1; }
        void MovePS2ExitNodeForAnalysis(void*,float) override { throw std::logic_error("EE accumulator fixture is not qualified"); }
        void InvokeClimbingNodeForAnalysis(void* n,unsigned v) override { Require(n==Pointer(0x600)&&!v,"node callback");Event("node:0"); }
        void* FindMovementNodeForAnalysis(void*,const char* name,bool r,bool a) override
        {Require(std::string(name)=="movement_tracker"&&r&&!a,"tracker lookup");Event("find");return found?Pointer(0x500):nullptr;}
        float ReadNodePositionWordForAnalysis(void* n,unsigned o) override { return (n==Pointer(0x500)?tracker:target)[(o-0x20)/4]; }
        void WriteNodePositionWordForAnalysis(void* n,unsigned o,float v) override { (n==Pointer(0x500)?tracker:target)[(o-0x20)/4]=v; }
        void MarkMovementNodeDirtyForAnalysis(void* n) override { (n==Pointer(0x500)?trackerDirty:targetDirty)|=1; }
        void* OwnerMovementTargetForAnalysis(void*) override { return Pointer(0x600); }
        Matrix3 ReadMovementTargetOrientationForAnalysis(void*) override { return {1,0,0,0,1,0,0,0,1}; }
        wxCharacterMovementNumericProfileForAnalysis MovementNumericProfileForAnalysis() const noexcept override {return ps2?wxCharacterMovementNumericProfileForAnalysis::PS2Finite:wxCharacterMovementNumericProfileForAnalysis::PCFinite;}
        void* ResolveAnimationForAnalysis(void*,unsigned key) override {Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override {Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override {Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,unsigned fade,bool interrupt) override {Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override {Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float v) override {Require(v==0.4f,"fade value");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {Require(consume,"consuming query");const auto token=unsigned(Token(h));Event("query:"+std::to_string(token));const bool result=!token||first==token||second==token;if(first==token)first=0;if(second==token)second=0;return result;}
        void ClearOwnerActionControlForAnalysis(void*) override {Event("word");motion=0;}
    };
    std::string Case(const std::uint64_t* v)
    {
        wxVineClimbingState state;wxAnimationRequestForAnalysis request{unsigned(v[2])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[3]));state.SetTransitionFlagsForAnalysis(v[8]&1,v[8]&2,v[8]&4,v[8]&8,v[8]&16);
        state.SetMovementFieldsForAnalysis(v[15]==2?Pointer(0x500):nullptr,v[15]==2,{1,2,3});
        host.ps2=v[0]!=0;host.next=unsigned(v[4]);host.predicate=v[5]!=0;host.first=unsigned(v[6]);host.second=unsigned(v[7]);host.motion=unsigned(v[9]);host.angleA=unsigned(v[10]);host.angleB=unsigned(v[11]);host.byte50=unsigned(v[12]);host.heightA=unsigned(v[13]);host.heightB=unsigned(v[14]);host.found=v[15]!=0;host.entityFlag=unsigned(v[16]);
        unsigned result=2;if(v[1]==0x1C)result=state.vfunc_1C(request);else if(v[1]==0x20)result=state.vfunc_20(request);else if(v[1]==0x30)state.vfunc_30(request);else throw std::logic_error("Vine slot");
        const auto m=state.GetMovementFieldsForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Flags(state)<<' '<<host.first<<' '<<host.second<<' '<<host.motion<<' '<<host.entityFlag<<' '<<m.field2C<<' '<<(m.field28!=nullptr);
        for(float x:m.resetValues)out<<' '<<Bits(x);for(float x:host.tracker)out<<' '<<Bits(x);for(float x:host.target)out<<' '<<Bits(x);out<<' '<<host.trackerDirty<<' '<<host.targetDirty<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==19&&std::string(argv[1])=="--case"){std::uint64_t v[17]{};for(unsigned i=0;i<17;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxVineClimbingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(state.GetStateSelectorForAnalysis()==16,"selector");Require(dynamic_cast<wxVineClimbingState*>(spRTTIManager::Instance().Create(wxVineClimbingState::ClassID).get()),"factory");
    Require(!state.vfunc_1C(request)&&request.packedKey==0xF027801F&&!(Flags(state)&1),"entry key and flag");
    host.first=host.second=22;Require(state.vfunc_1C(request),"completed entry");
    state.vfunc_40_ResetForAnalysis();host.byte50=0;host.heightA=Bits(51);host.heightB=0;host.motion=Bits(1);request.packedKey=0xFFFFFFFF;
    Require(state.vfunc_20(request)&&Flags(state)&4&&host.motion==Bits(1),"early exit keeps once flag and control word");Require(host.target[2]==10,"early exit delta");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxVineClimbingState*>(clone.get());Require(fresh&&Flags(*fresh)==31&&!fresh->GetPendingHandleForAnalysis(),"fresh clone");
    std::cout<<"VineClimbing checks passed\n";
}
