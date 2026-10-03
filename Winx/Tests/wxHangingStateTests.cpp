#include "Code/wxHangingState.h"
#include "Analysis/Host/wxHangingStateHost.h"
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
    struct Host final : wxHangingStateHost
    {
        wxHangingState& state; wxAnimationRequestForAnalysis& request;
        bool ps2=false,predicate=false,found=false;
        unsigned next=22,first=11,second=11,motion=0x3F800000,entityMotion=0x3F800000,angle=0x3F800000,exitAngle=0x3F800000,kind=1,pair0=0,pair1=0x3F800000,word=0,eligible=1,entityAngle=0,entityFlag=7,byte1B=9,speed=0,managerWord=7;
        bool pairPresent=true;
        Vector3 finalPosition{101,102,103};
        Vector3 tracker{3,4,5},target{10,20,30}; unsigned trackerDirty=0xA4,targetDirty=0xA4;
        std::string trace;
        Host(wxHangingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r) {}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';auto m=state.GetMovementFieldsForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)+':'+std::to_string(Flags(state))+':'+std::to_string(entityFlag)+':'+std::to_string(m.field2C)+':'+std::to_string(m.field28!=nullptr)+':'+std::to_string(state.GetField3CForAnalysis())+':'+std::to_string(byte1B);
        }
        void ClearGameManagerWord504ForAnalysis() override {Event("manager");managerWord=0;}
        unsigned OwnerKindForAnalysis(void*) override {return kind;}
        std::optional<std::array<float,2>> EntryComparePairForAnalysis(void*) override
        {return pairPresent?std::optional<std::array<float,2>>{{Float(pair0),Float(pair1)}}:std::nullopt;}
        void RotateHangingNodeForAnalysis(void* n,float a) override {Require(n==Pointer(0x600),"rotation node");Event("rotate:"+std::to_string(Bits(a)));}
        float UpdateMotionForAnalysis(void*) override {return Float(motion);}
        float UpdateAngleForAnalysis(void*) override {return Float(angle);}
        void SetConsumerSpeedForAnalysis(void*,float v) override {Event("speed:"+std::to_string(Bits(v)));speed=Bits(v);}
        void* OwnerEntityControlForAnalysis(void*) override {return Pointer(0x700);}
        void WriteEntityFlagForAnalysis(void* c,std::uint8_t v) override {Require(c==Pointer(0x700),"entity control");Event("entity-flag:"+std::to_string(v));entityFlag=v;}
        unsigned ExitPackedWordForAnalysis(void*) override {return word;}
        float ExitControlAngleForAnalysis(void*) override {return Float(exitAngle);}
        std::uint8_t ExitEligibilityByteForAnalysis(void*) override {return std::uint8_t(eligible);}
        float ExitEntityAngleForAnalysis(void*) override {return Float(entityAngle);}
        void WriteDirectControlByte1BForAnalysis(void*,std::uint8_t v) override {Event("byte1B:"+std::to_string(v));byte1B=v;}
        void SetFinalControlPositionForAnalysis(void* c,const Vector3& position) override
        {Require(c==Pointer(0x700),"final control");Event("position:"+std::to_string(Bits(position[0]))+':'+std::to_string(Bits(position[1]))+':'+std::to_string(Bits(position[2])));finalPosition=position;}
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
        wxHangingState state;wxAnimationRequestForAnalysis request{unsigned(v[2])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[3]));state.SetTransitionFlagsForAnalysis(v[8]&1,v[8]&2,v[8]&4,v[8]&8,v[8]&16);
        state.SetMovementFieldsForAnalysis(v[21]==2?Pointer(0x500):nullptr,v[21]==2,{1,2,3});state.SetField3CForAnalysis(unsigned(v[16]));
        host.ps2=v[0]!=0;host.next=unsigned(v[4]);host.predicate=v[5]!=0;host.first=unsigned(v[6]);host.second=unsigned(v[7]);host.motion=unsigned(v[9]);host.angle=unsigned(v[10]);host.exitAngle=unsigned(v[11]);host.kind=unsigned(v[12]);host.pairPresent=v[13]!=0;host.pair0=unsigned(v[14]);host.pair1=unsigned(v[15]);host.entityMotion=unsigned(v[17]);host.word=unsigned(v[18]);host.eligible=unsigned(v[19]);host.entityAngle=unsigned(v[20]);host.found=v[21]!=0;host.entityFlag=unsigned(v[22]);host.byte1B=unsigned(v[23]);host.speed=unsigned(v[24]);host.managerWord=unsigned(v[25]);
        unsigned result=2;if(v[1]==0x1C)result=state.vfunc_1C(request);else if(v[1]==0x20)result=state.vfunc_20(request);else if(v[1]==0x30)state.vfunc_30(request);else throw std::logic_error("Hanging slot");
        const auto m=state.GetMovementFieldsForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<Flags(state)<<' '<<host.first<<' '<<host.second<<' '<<host.motion<<' '<<host.entityFlag<<' '<<host.byte1B<<' '<<state.GetField3CForAnalysis()<<' '<<host.speed<<' '<<host.managerWord<<' '<<m.field2C<<' '<<(m.field28!=nullptr);
        for(float x:m.resetValues)out<<' '<<Bits(x);for(float x:host.tracker)out<<' '<<Bits(x);for(float x:host.target)out<<' '<<Bits(x);out<<' '<<host.trackerDirty<<' '<<host.targetDirty;for(float x:host.finalPosition)out<<' '<<Bits(x);out<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==28&&std::string(argv[1])=="--case"){std::uint64_t v[26]{};for(unsigned i=0;i<26;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxHangingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
    Require(state.GetStateSelectorForAnalysis()==17&&!state.GetField3CForAnalysis(),"constructor");Require(dynamic_cast<wxHangingState*>(spRTTIManager::Instance().Create(wxHangingState::ClassID).get()),"factory");
    state.SetField3CForAnalysis(9);Require(!state.vfunc_1C(request)&&!state.GetField3CForAnalysis()&&!host.managerWord&&!(Flags(state)&1),"first entry resets count and manager");
    host.first=host.second=22;Require(state.vfunc_1C(request)&&state.GetField3CForAnalysis()==1,"completed entry invokes own update");
    state.SetField3CForAnalysis(0xFFFFFFFF);host.angle=0;host.motion=Bits(1);state.vfunc_30(request);Require(!state.GetField3CForAnalysis()&&host.speed==Bits(1.3f),"counter wraps and speed boost");
    state.SetField3CForAnalysis(42);state.vfunc_40_ResetForAnalysis();Require(state.GetField3CForAnalysis()==42,"base reset leaves count");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxHangingState*>(clone.get());Require(fresh&&!fresh->GetField3CForAnalysis()&&Flags(*fresh)==31,"fresh clone");
    wxHangingState target;target.SetField3CForAnalysis(71);spCloneManager manager;Require(state.vfunc_14(target,manager)&&target.GetField3CForAnalysis()==71,"empty copy");
    state.SetTransitionFlagsForAnalysis(false,false,false,true,true);state.SetPendingHandleForAnalysis(Pointer(11));host.first=host.second=11;host.motion=Bits(1);host.trace.clear();
    Require(state.vfunc_20(request)&&!host.motion&&host.speed==Bits(1)&&host.finalPosition==host.target,"completed exit captures position and clears motion");
    std::cout<<"Hanging checks passed\n";
}
