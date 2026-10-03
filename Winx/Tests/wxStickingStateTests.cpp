#include "Code/wxStickingLeftState.h"
#include "Code/wxStickingRightState.h"
#include "Analysis/Host/wxStickingStateHost.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value, const char* message)
    { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    float Float(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4); return value; }
    std::uint32_t Bits(float value) { std::uint32_t bits; std::memcpy(&bits,&value,4); return bits; }
    unsigned Flags(const wxCharacterState& state)
    { unsigned result=0;const auto flags=state.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(flags[i])result|=1u<<i;return result; }
    struct Host final : wxStickingStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        Vector3 tracker{3,4,5},target{10,20,30};Matrix3 matrix{1,0,0,0,1,0,0,0,1};
        std::uint32_t trackerDirty=0xA4,targetDirty=0xA4,word=7;
        std::uintptr_t next=22,first=0,second=0;bool lookupPresent=true,predicate=false,ps2=false;
        std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';const auto cache=state.GetMovementFieldsForAnalysis();
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey)
                +':'+std::to_string(Flags(state))+':'+std::to_string(bool(cache.field28))+':'+std::to_string(cache.field2C);
            for(float value:cache.resetValues) trace+=':'+std::to_string(Bits(value));
        }
        unsigned entryWord=0,exitWord=0,speed=0;float angle=0;
        unsigned ReadEntryOwnerWordForAnalysis(void*) override {Event("entryword");return entryWord;}
        unsigned ReadExitOwnerWordForAnalysis(void*) override {Event("exitword");return exitWord;}
        wxStickingObjectsForAnalysis CaptureStickingObjectsForAnalysis(void*) override {Event("objects");return {Pointer(0x700),Pointer(0x800)};}
        float ReadStickingAngleForAnalysis(void* object) override {Require(object==Pointer(0x700),"captured angle");Event("angle");return angle;}
        float ReadStickingMotionForAnalysis(void* object) override {Require(object==Pointer(0x800),"captured control");Event("motion");return Float(word);}
        void SetConsumerSpeedForAnalysis(void* consumer,float value) override {Require(consumer==Pointer(0x200),"borrowed consumer");Event("speed:"+std::to_string(Bits(value)));speed=Bits(value);}
        wxCharacterMovementNumericProfileForAnalysis MovementNumericProfileForAnalysis() const noexcept override
        { return ps2?wxCharacterMovementNumericProfileForAnalysis::PS2Finite:wxCharacterMovementNumericProfileForAnalysis::PCFinite; }
        void* FindMovementNodeForAnalysis(void*,const char* name,bool recursive,bool last) override
        { Require(std::string(name)=="movement_tracker" && recursive && !last,"exact lookup arguments");Event("find");return lookupPresent?Pointer(0x500):nullptr; }
        float ReadNodePositionWordForAnalysis(void* node,unsigned offset) override
        { Require(offset==0x20 || offset==0x24 || offset==0x28,"position word");Event("read:"+std::to_string(Token(node))+':'+std::to_string(offset));return (node==Pointer(0x500)?tracker:target)[(offset-0x20)/4]; }
        void WriteNodePositionWordForAnalysis(void* node,unsigned offset,float value) override
        { Event("write:"+std::to_string(Token(node))+':'+std::to_string(offset)+':'+std::to_string(Bits(value)));(node==Pointer(0x500)?tracker:target)[(offset-0x20)/4]=value; }
        void MarkMovementNodeDirtyForAnalysis(void* node) override
        { Event("dirty:"+std::to_string(Token(node)));(node==Pointer(0x500)?trackerDirty:targetDirty)|=1; }
        void* OwnerMovementTargetForAnalysis(void*) override { Event("target");return Pointer(0x600); }
        Matrix3 ReadMovementTargetOrientationForAnalysis(void*) override { Event("matrix");return matrix; }
        void* ResolveAnimationForAnalysis(void*,unsigned key) override { Event("lookup:"+std::to_string(key));return Pointer(next); }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate");return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override
        { Event("reset:"+std::to_string(Token(handle)));if(first==Token(handle))first=0;if(second==Token(handle))second=0; }
        void StartAnimationForAnalysis(void*,void* handle,bool mode,unsigned fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*,void* handle) override { Event("stop:"+std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override
        { Require(duration==0.4f,"fade duration");Event("fade:"+std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {
            Require(consume,"consuming query");const auto token=Token(handle);Event("query:"+std::to_string(token));
            if(!token)return true;const bool done=first==token || second==token;
            if(first==token)first=0;if(second==token)second=0;return done;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word");word=0; }
    };
    std::string Case(const std::uint64_t* v)
    {
        wxStickingLeftState left;wxStickingRightState right;wxCharacterState& state=v[1]?static_cast<wxCharacterState&>(right):left;
        wxAnimationRequestForAnalysis request{std::uint32_t(v[3])};Host host(state,request);state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[4]));
        state.SetTransitionFlagsForAnalysis(v[9]&1,v[9]&2,v[9]&4,v[9]&8,v[9]&16);
        std::array<float,3> previous{};for(unsigned i=0;i<3;++i)previous[i]=Float(unsigned(v[17+i]));state.SetMovementFieldsForAnalysis(v[15]?Pointer(0x500):nullptr,v[14]!=0,previous);
        host.ps2=v[0]!=0;host.next=v[5];host.predicate=v[6]!=0;host.first=v[7];host.second=v[8];host.entryWord=unsigned(v[10]);host.exitWord=unsigned(v[11]);host.angle=Float(unsigned(v[12]));host.word=unsigned(v[13]);host.lookupPresent=v[16]!=0;host.speed=unsigned(v[35]);
        for(unsigned i=0;i<3;++i){host.tracker[i]=Float(unsigned(v[20+i]));host.target[i]=Float(unsigned(v[23+i]));}for(unsigned i=0;i<9;++i)host.matrix[i]=Float(unsigned(v[26+i]));
        unsigned result=2;if(v[2]==0x1C)result=state.vfunc_1C(request);else if(v[2]==0x20)result=state.vfunc_20(request);else if(v[2]==0x30)state.vfunc_30(request);else throw std::logic_error("sticking slot");
        std::ostringstream out;const auto cache=state.GetMovementFieldsForAnalysis();out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<' '<<bool(cache.field28)<<' '<<cache.field2C<<' '<<host.word<<' '<<host.speed;
        for(float value:cache.resetValues)out<<' '<<Bits(value);for(float value:host.tracker)out<<' '<<Bits(value);out<<' '<<host.trackerDirty;for(float value:host.target)out<<' '<<Bits(value);out<<' '<<host.targetDirty<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==38 && std::string(argv[1])=="--case")
    {std::uint64_t v[36]{};for(unsigned i=0;i<36;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS;}
    wxStickingLeftState left;wxStickingRightState right;Require(left.GetStateSelectorForAnalysis()==12 && right.GetStateSelectorForAnalysis()==13,"selectors");
    Require(dynamic_cast<wxStickingLeftState*>(spRTTIManager::Instance().Create(wxStickingLeftState::ClassID).get()) && dynamic_cast<wxStickingRightState*>(spRTTIManager::Instance().Create(wxStickingRightState::ClassID).get()),"factories");
    wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(left,request);left.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);left.SetPendingHandleForAnalysis(Pointer(11));host.entryWord=5;
    Require(!left.vfunc_1C(request) && request.packedKey==0xF0A7803F && left.GetPendingHandleForAnalysis()==Pointer(22) && !(Flags(left)&1),"entry variant and once");Require(host.trace.find("stop:")==std::string::npos,"entry leaves old pending unreleased");
    host.first=host.second=22;Require(left.vfunc_1C(request),"completed entry dispatches own update");
    host.exitWord=5;Require(left.vfunc_20(request) && (Flags(left)&4) && host.trace.find("find")==std::string::npos,"exit shortcut preserves once and skips movement prepare");
    wxAnimationRequestForAnalysis r{0xFFFFFFFF};Host h(right,r);right.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&h);h.exitWord=0x100000;Require(!right.vfunc_20(r) && !(Flags(right)&4) && right.GetMovementFieldsForAnalysis().field28,"right exit prepares movement and queues variant");
    Require((r.packedKey&0xF800000)==0x800000 && !(h.word),"right exit reads extra variant and clears control");
    h.angle=0.5f;h.word=Bits(1.0f);r.packedKey=0;right.vfunc_30(r);Require(h.speed==Bits(1.3f) && r.packedKey==0x800040,"right angle speed boost");
    host.angle=Float(0x4016CBE4);host.word=Bits(1.0f);request.packedKey=0;left.vfunc_30(request);Require(request.packedKey==0x30,"PC left compares unspilled triple threshold");
    host.ps2=true;request.packedKey=0;left.vfunc_30(request);Require(request.packedKey==0x800030,"PS2 left threshold rounds separately");
    auto clone=right.Clone();auto* fresh=dynamic_cast<wxStickingRightState*>(clone.get());Require(fresh && fresh->GetStateSelectorForAnalysis()==13 && !fresh->GetPendingHandleForAnalysis() && Flags(*fresh)==31,"fresh clone");
    std::cout<<"Sticking state checks passed\n";
}
