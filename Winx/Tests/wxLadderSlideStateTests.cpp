#include "Code/wxLadderSlideState.h"
#include "Analysis/Host/wxLadderSlideStateHost.h"
#include "Analysis/PC/wxLadderSlideStateAbi.h"
#include "Analysis/PS2/wxLadderSlideStateAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
    void* Pointer(std::uintptr_t v){return reinterpret_cast<void*>(v);}
    std::uintptr_t Token(void* p){return reinterpret_cast<std::uintptr_t>(p);}
    unsigned Flags(const wxCharacterState& state){unsigned value=0;auto flags=state.GetTransitionFlagsForAnalysis();for(unsigned i=0;i<5;++i)if(flags[i])value|=1u<<i;return value;}
    struct Host final : wxLadderSlideStateHost
    {
        wxLadderSlideState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;std::uint32_t word=0x12345678;
        unsigned ownerFlags=0,predicate=0,mutation=0;std::string trace;
        Host(wxLadderSlideState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& e){if(!trace.empty())trace+=';';trace+=e+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags(state))+':'+std::to_string(request.packedKey);}
        std::uint8_t OwnerLadderExitFlagsForAnalysis(void*) override{Event("owner-flags");return static_cast<std::uint8_t>(ownerFlags);}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));if(mutation==1)state.SetPendingHandleForAnalysis(Pointer(77));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");if(mutation==4)state.SetPendingHandleForAnalysis(Pointer(77));return predicate!=0;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));if(first==Token(h))first=0;if(second==Token(h))second=0;}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override{Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));if(mutation==2)state.SetPendingHandleForAnalysis(Pointer(99));}
        void Released(){if(mutation==3){request.packedKey=0xabcdef12;state.SetPendingHandleForAnalysis(Pointer(99));}}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));Released();}
        void FadeAnimationForAnalysis(void*,void* h,float duration) override{Check(duration==0.4f,"fade literal");Event("fade:"+std::to_string(Token(h)));Released();}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override{Check(consume,"consuming query");Event("query:"+std::to_string(Token(h)));if(!h)return true;bool hit=first==Token(h)||second==Token(h);if(first==Token(h))first=0;if(second==Token(h))second=0;return hit;}
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    unsigned Call(wxLadderSlideState& state,wxAnimationRequestForAnalysis& request,unsigned slot)
    {
        if(slot==0x1c)return state.vfunc_1C(request);
        if(slot==0x20)return state.vfunc_20(request);
        if(slot==0x30){state.vfunc_30(request);return 2;}
        if(slot==0x34)return state.vfunc_34(request.packedKey);
        if(slot==0x38)return state.vfunc_38(request.packedKey);
        if(slot==0x40){state.vfunc_40_ResetForAnalysis();return 2;}
        throw std::runtime_error("slot");
    }
    std::string Snapshot(unsigned result,const wxLadderSlideState& state,const wxAnimationRequestForAnalysis& request,const Host& host)
    {std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(state)<<'|'<<host.trace;return out.str();}
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,std::uintptr_t first,std::uintptr_t second,unsigned flags,unsigned ownerFlags,unsigned predicate,unsigned mutation)
    {
        wxLadderSlideState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.first=first;host.second=second;host.ownerFlags=ownerFlags;host.predicate=predicate;host.mutation=mutation;
        const auto result=Call(state,request,slot);return Snapshot(result,state,request,host);
    }
    std::string Sequence(unsigned predicate,unsigned mutation)
    {
        wxLadderSlideState state;wxAnimationRequestForAnalysis request{0xffffffff};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);host.next=22;host.first=22;host.second=22;host.ownerFlags=1;host.predicate=predicate;host.mutation=mutation;
        std::string result;
        // One owner/consumer transaction: first entry, incomplete entry,
        // completion, unchanged update, first exit, incomplete exit,
        // completion, reset. Completion is delivered by the foreign consumer.
        for(unsigned step=0;step<8;++step)
        {
            if(step==2||step==6)host.first=host.second=Token(state.GetPendingHandleForAnalysis());
            const unsigned slot=step<3?0x1c:step==3?0x30:step<7?0x20:0x40;
            host.trace.clear();const auto value=Call(state,request,slot);
            if(!result.empty())result+='\n';result+=Snapshot(value,state,request,host);
        }
        return result;
    }
    void Lifecycle()
    {
        wxLadderSlideState source;Check(source.IsExactly(wxLadderSlideState::ClassID)&&source.IsKindOf(wxCharacterState::ClassID),"RTTI/base");Check(source.GetStateSelectorForAnalysis()==15&&Flags(source)==31&&!source.GetPendingHandleForAnalysis(),"constructor defaults");
        Check(dynamic_cast<wxLadderSlideState*>(spRTTIManager::Instance().Create(wxLadderSlideState::ClassID).get()),"factory");
        source.SetPendingHandleForAnalysis(Pointer(11));source.SetTransitionFlagsForAnalysis(false,false,false,false,false);auto clone=source.Clone();auto* target=dynamic_cast<wxLadderSlideState*>(clone.get());Check(target&&Flags(*target)==31&&!target->GetPendingHandleForAnalysis()&&target->GetStateSelectorForAnalysis()==15,"fresh clone state");
        spCloneManager manager;Check(source.vfunc_14(*target,manager)&&Flags(*target)==31,"empty inherited copy");
        wxLadderSlideState unbound;Check(unbound.vfunc_34(0)&&unbound.vfunc_34(10)&&unbound.vfunc_34(14)&&!unbound.vfunc_34(15)&&unbound.vfunc_38(0)==1,"permission hooks without owner");bool threw=false;wxAnimationRequestForAnalysis request{};try{(void)unbound.vfunc_20(request);}catch(const std::logic_error&){threw=true;}Check(threw,"missing owner host is error");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==12&&std::string(argv[1])=="--case")
        {std::uint64_t v[10]{};for(unsigned i=0;i<10;++i)v[i]=std::stoull(argv[i+2]);std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4],v[5],unsigned(v[6]),unsigned(v[7]),unsigned(v[8]),unsigned(v[9]))<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--sequence"){std::cout<<Sequence(unsigned(std::stoul(argv[2])),unsigned(std::stoul(argv[3])))<<'\n';return 0;}
        Lifecycle();
        Check(Case(0x1c,0xffffffff,11,22,11,22,31,1,0,0).rfind("0 ",0)==0,"entry waits");
        Check(Case(0x20,0xffffffff,11,22,11,22,31,0x10,0,0).rfind("1 ",0)==0,"high owner bits skip special exit");
        Check(Case(0x20,0xffffffff,11,22,11,22,31,1,0,3).find("lookup:2898267938")!=std::string::npos,"exit key follows release callback");
        Check(Case(0x30,0xffffffff,22,22,22,22,31,1,0,0).find("start:")==std::string::npos,"unchanged update queues nothing");
        Check(Sequence(0,0).find("query:22")!=std::string::npos,"entry/exit completion transaction");
        std::cout<<"PASS LadderSlide lifecycle, entry/update/exit transaction and owner nibble gate\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
