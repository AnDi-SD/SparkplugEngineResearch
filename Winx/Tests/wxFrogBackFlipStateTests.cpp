#include "Code/wxFrogBackFlipState.h"
#include "Analysis/Host/wxCharacterMovementStateHost.h"
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
    struct StateFixture final : wxCharacterState
    {
        using wxCharacterState::PrepareMovementFromState;
        using wxCharacterState::FinishMovementFromState;
    };
    struct Host final : wxCharacterMovementStateHost
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
                +':'+std::to_string(bool(cache.field28))+':'+std::to_string(cache.field2C);
            for(float value:cache.resetValues) trace+=':'+std::to_string(Bits(value));
        }
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
        StateFixture base;wxFrogBackFlipState frog;wxCharacterState& state=v[0]<2?static_cast<wxCharacterState&>(base):frog;
        wxAnimationRequestForAnalysis request{std::uint32_t(v[1])};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(v[2]));
        std::array<float,3> previous{};for(unsigned i=0;i<3;++i)previous[i]=Float(std::uint32_t(v[13+i]));
        state.SetMovementFieldsForAnalysis(v[8]?Pointer(0x500):nullptr,v[7]!=0,previous);
        host.next=v[3];host.predicate=v[4]!=0;host.first=v[5];host.second=v[6];host.lookupPresent=v[9]!=0;host.ps2=v[12]!=0;
        for(unsigned i=0;i<3;++i){host.tracker[i]=Float(std::uint32_t(v[16+i]));host.target[i]=Float(std::uint32_t(v[19+i]));}
        for(unsigned i=0;i<9;++i)host.matrix[i]=Float(std::uint32_t(v[22+i]));
        unsigned result=2;
        if(v[0]==0)base.PrepareMovementFromState(v[10]!=0);
        else if(v[0]==1)base.FinishMovementFromState(v[11]!=0);
        else if(v[0]==2)result=frog.vfunc_1C(request);
        else if(v[0]==3)frog.vfunc_30(request);
        else if(v[0]==4)result=frog.vfunc_34(request.packedKey);
        else throw std::logic_error("movement case kind");
        const auto cache=state.GetMovementFieldsForAnalysis();std::ostringstream out;
        out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.first<<' '<<host.second
            <<' '<<bool(cache.field28)<<' '<<cache.field2C<<' '<<host.word;
        for(float x:cache.resetValues)out<<' '<<Bits(x);
        for(float x:host.tracker)out<<' '<<Bits(x);out<<' '<<host.trackerDirty;
        for(float x:host.target)out<<' '<<Bits(x);out<<' '<<host.targetDirty<<'|'<<host.trace;return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==33 && std::string(argv[1])=="--case")
    { std::uint64_t v[31]{};for(unsigned i=0;i<31;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(v)<<'\n';return EXIT_SUCCESS; }
    StateFixture state;wxAnimationRequestForAnalysis request{};Host host(state,request);state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);
    state.PrepareMovementFromState(false);Require(state.GetMovementFieldsForAnalysis().field28==Pointer(0x500)
        && state.GetMovementFieldsForAnalysis().field2C && host.tracker==Host::Vector3{} && host.trackerDirty==0xA5,"prepare lookup, zero and dirty");
    host.trace.clear();state.PrepareMovementFromState(false);Require(host.trace.find("find")==std::string::npos,"prepare caches lookup");
    host.tracker={3,4,5};state.FinishMovementFromState(false);
    Require(host.target==Host::Vector3{7,24,25} && state.GetMovementFieldsForAnalysis().resetValues==host.tracker
        && host.targetDirty==0xA5,"rotated finish uses (-X,+Y,-Z) delta and updates shared cache");
    const auto position=host.target;state.FinishMovementFromState(false);Require(host.target==position,"second finish sees cached delta zero");
    host.tracker={6,8,10};state.FinishMovementFromState(true);Require(host.target==Host::Vector3{7,28,25},"vertical finish changes Y only");
    state.PrepareMovementFromState(true);Require(!state.GetMovementFieldsForAnalysis().field28 && !state.GetMovementFieldsForAnalysis().field2C,"clear resets cached lookup after node zero");
    host.lookupPresent=false;state.PrepareMovementFromState(false);host.trace.clear();state.PrepareMovementFromState(false);
    Require(host.trace.empty() && state.GetMovementFieldsForAnalysis().field2C,"null result is cached too");
    state.SetMovementFieldsForAnalysis(Pointer(0x500),true,{1,2,3});state.vfunc_40_ResetForAnalysis();
    Require(!state.GetMovementFieldsForAnalysis().field28 && !state.GetMovementFieldsForAnalysis().field2C
        && state.GetMovementFieldsForAnalysis().resetValues==Host::Vector3{},"base Reset clears actual movement cache");
    wxFrogBackFlipState frog;wxAnimationRequestForAnalysis flip{0xFFFFFFFF};Host frogHost(frog,flip);
    frog.SetBindingsForAnalysis(Pointer(1),Pointer(2),&frogHost);frog.SetPendingHandleForAnalysis(Pointer(11));
    Require(frog.vfunc_1C(flip) && flip.packedKey==0xF0080850 && frog.GetPendingHandleForAnalysis()==Pointer(22)
        && frogHost.trace.find("stop:")==std::string::npos,"backflip entry prepares and queues without old release");
    frogHost.first=frogHost.second=22;Require(frog.vfunc_34(10) && !frogHost.first && !frogHost.second,"permission consumes both completion records");
    frogHost.tracker={3,4,5};frogHost.trace.clear();frog.vfunc_30(flip);
    Require(frogHost.target==Host::Vector3{7,24,25} && frogHost.word==0
        && frogHost.trace.rfind("dirty:1536")<frogHost.trace.rfind("word@"),"backflip update applies movement before control clear");
    frogHost.trace.clear();frog.vfunc_30(flip);
    Require(frogHost.target==Host::Vector3{7,24,25},"backflip repeated update consumes no movement twice");
    frog.SetPendingHandleForAnalysis(nullptr);frogHost.trace.clear();
    Require(frog.vfunc_34(0) && frogHost.trace.find("query:0")!=std::string::npos,"backflip null completion still queried");
    frogHost.ps2=true;frogHost.tracker={6,8,10};bool requiresEE=false;
    try { frog.vfunc_30(flip); } catch (const std::logic_error&) { requiresEE=true; }
    Require(requiresEE,"PS2 rotated movement requires explicit EE service");
    auto clone=frog.Clone();auto* fresh=dynamic_cast<wxFrogBackFlipState*>(clone.get());
    Require(fresh && fresh->GetStateSelectorForAnalysis()==25 && !fresh->GetMovementFieldsForAnalysis().field28
        && !fresh->GetPendingHandleForAnalysis(),"clone has fresh movement cache");
    Require(dynamic_cast<wxFrogBackFlipState*>(spRTTIManager::Instance().Create(wxFrogBackFlipState::ClassID).get()),"factory");
    std::cout<<"Movement cache and FrogBackFlip checks passed\n";
}
