#include "Analysis/Host/wxBirdMovingStateHost.h"
#include "Code/wxBirdMovingState.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value,const char* message)
    { if(!value){std::cerr<<message<<'\n';std::exit(EXIT_FAILURE);} }
    void* Pointer(std::uintptr_t value){return reinterpret_cast<void*>(value);}
    std::uintptr_t Token(void* value){return reinterpret_cast<std::uintptr_t>(value);}
    struct Host final:wxBirdMovingStateHost
    {
        wxBirdMovingState& state;
        wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22);
        bool predicate=false;
        std::uint32_t random[2]{0,0};unsigned draws=0;
        std::uintptr_t first=0,second=0;
        std::string trace,tag="unknown";
        Host(wxBirdMovingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(state.GetCycleCountForAnalysis())+':'+std::to_string(state.GetNeedsPlaybackForAnalysis())
                +':'+std::to_string(request.packedKey);
        }
        std::uint32_t DrawRandomForAnalysis() override
        {Require(draws<2,"bounded RNG sequence");Event("random:"+std::to_string(random[draws]));return random[draws++];}
        const char* EventTagNameForAnalysis(const void*) override{return tag.c_str();}
        void JumpFromEventForAnalysis(void*) override{Event("jump");}
        void LandFromEventForAnalysis(void*) override{Event("land");}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        {Event("lookup:"+std::to_string(key));return next;}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* handle) override{Event("reset:"+std::to_string(Token(handle)));}
        void StartAnimationForAnalysis(void*,void* handle,bool mode,std::uint32_t fade,bool interrupt) override
        {Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* handle) override{Event("stop:"+std::to_string(Token(handle)));}
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override
        {Require(duration==0.4f,"native fade");Event("fade:"+std::to_string(Token(handle)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {
            Require(consume,"consume completion");const auto token=Token(handle);Event("query:"+std::to_string(token));
            if(!token)return true;
            const bool complete=first==token || second==token;
            if(first==token)first=0;if(second==token)second=0;return complete;
        }
        void ClearOwnerActionControlForAnalysis(void*) override{throw std::logic_error("unexpected control clear");}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        bool predicate,std::uint32_t count,bool needs,std::uint32_t r0,std::uint32_t r1,
        std::uintptr_t first,std::uintptr_t second,const std::string& tag)
    {
        wxBirdMovingState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetCycleForAnalysis(count,needs);host.next=Pointer(next);host.predicate=predicate;
        host.random[0]=r0;host.random[1]=r1;host.first=first;host.second=second;host.tag=tag;unsigned result=2;
        if(slot==0x1C)result=state.vfunc_1C(request);
        else if(slot==0x30)state.vfunc_30(request);
        else if(slot==0x3C)state.vfunc_3C(nullptr);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;output<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())
            <<' '<<state.GetCycleCountForAnalysis()<<' '<<state.GetNeedsPlaybackForAnalysis()<<' '<<host.draws
            <<' '<<host.first<<' '<<host.second<<'|'<<host.trace;return output.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==14 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[11]{};for(unsigned i=0;i<11;++i)v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,std::uint32_t(v[5]),v[6]!=0,
            std::uint32_t(v[7]),std::uint32_t(v[8]),v[9],v[10],argv[13])<<'\n';return EXIT_SUCCESS;
    }
    wxBirdMovingState state;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);
    Require(state.GetStateSelectorForAnalysis()==0 && state.GetCycleCountForAnalysis()==0
        && state.GetNeedsPlaybackForAnalysis() && state.IsExactly(state.ClassID) && state.IsKindOf(wxCharacterState::ClassID),
        "constructor tail, selector and RTTI");
    Require(dynamic_cast<wxBirdMovingState*>(spRTTIManager::Instance().Create(state.ClassID).get()),"factory");
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);host.random[0]=2;host.random[1]=3;
    Require(state.vfunc_1C(request) && state.GetCycleCountForAnalysis()==4 && !state.GetNeedsPlaybackForAnalysis()
        && host.draws==2 && state.GetPendingHandleForAnalysis()==Pointer(22),"entry performs two-draw branch and queues");
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxBirdMovingState*>(clone.get());
    Require(fresh && fresh->GetCycleCountForAnalysis()==0 && fresh->GetNeedsPlaybackForAnalysis()
        && !fresh->GetPendingHandleForAnalysis(),"empty Copy clone leaves tail at defaults");
    host.first=host.second=22;state.vfunc_30(request);
    Require(state.GetNeedsPlaybackForAnalysis() && state.GetCycleCountForAnalysis()==3 && !host.first && !host.second,
        "completion consumes every matching record and decrements once");
    state.vfunc_30(request);Require(host.draws==2 && !state.GetNeedsPlaybackForAnalysis(),"remaining cycles reuse key without RNG");
    state.vfunc_40_ResetForAnalysis();Require(state.GetCycleCountForAnalysis()==3 && !state.GetNeedsPlaybackForAnalysis(),
        "base reset does not clear own fields");
    host.trace.clear();host.tag="event_jump";state.vfunc_3C(nullptr);Require(host.trace.find("jump@")==0,"jump tag");
    host.trace.clear();host.tag="event_land";state.vfunc_3C(nullptr);Require(host.trace.find("land@")==0,"land tag");
    host.trace.clear();host.tag="event_jump_extra";state.vfunc_3C(nullptr);Require(host.trace.empty(),"full tag equality");
    std::cout<<"Bird moving state checks passed\n";
}
