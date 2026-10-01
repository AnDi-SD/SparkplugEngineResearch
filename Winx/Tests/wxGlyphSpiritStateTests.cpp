#include "Analysis/Host/wxGlyphStateHost.h"
#include "Code/wxGlyphState.h"
#include "Code/wxSpiritAwayState.h"

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
    {if(!value){std::cerr<<message<<'\n';std::exit(EXIT_FAILURE);}}
    void* Pointer(std::uintptr_t value){return reinterpret_cast<void*>(value);}
    std::uintptr_t Token(void* value){return reinterpret_cast<std::uintptr_t>(value);}
    struct Host final:wxGlyphStateHost
    {
        wxCharacterState& state;wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22);bool predicate=false;
        std::uint32_t word=0xFFFFFFFF,classification=15;
        std::uintptr_t first=0,second=0;std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        void Event(const std::string& name)
        {
            if(!trace.empty())trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(request.packedKey);
        }
        std::uint32_t ReadOwnerClassificationWordForAnalysis(void*) override
        {Event("classification:"+std::to_string(classification));return classification;}
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
            if(!token)return true;const bool complete=first==token || second==token;
            if(first==token)first=0;if(second==token)second=0;return complete;
        }
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");word=0;}
    };
    std::string Case(unsigned kind,unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,
        bool predicate,std::uint32_t classification,std::uintptr_t first,std::uintptr_t second)
    {
        std::unique_ptr<wxCharacterState> state;
        if(kind==0)state=std::make_unique<wxGlyphState>();else state=std::make_unique<wxSpiritAwayState>();
        wxAnimationRequestForAnalysis request{key};Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state->SetPendingHandleForAnalysis(Pointer(pending));
        host.next=Pointer(next);host.predicate=predicate;host.classification=classification;host.first=first;host.second=second;
        unsigned result=2;
        if(slot==0x1C)result=state->vfunc_1C(request);
        else if(slot==0x20)result=state->vfunc_20(request);
        else if(slot==0x30)state->vfunc_30(request);
        else if(slot==0x34)result=state->vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;output<<result<<' '<<request.packedKey<<' '<<Token(state->GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<'|'<<host.trace;return output.str();
    }
    template<class State> void CheckFactoryAndClone(unsigned selector)
    {
        State state;Require(state.GetStateSelectorForAnalysis()==selector && state.IsExactly(State::ClassID)
            && state.IsKindOf(wxCharacterState::ClassID),"constructor and RTTI");
        Require(dynamic_cast<State*>(spRTTIManager::Instance().Create(State::ClassID).get()),"registered factory");
        state.SetPendingHandleForAnalysis(Pointer(11));state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone();auto* fresh=dynamic_cast<State*>(clone.get());
        Require(fresh && fresh->GetStateSelectorForAnalysis()==selector && !fresh->GetPendingHandleForAnalysis()
            && fresh->GetTransitionFlagsForAnalysis()==std::array<bool,5>{true,true,true,true,true},"clone keeps constructor defaults");
        State target;spCloneManager manager;Require(state.vfunc_14(target,manager) && !target.GetPendingHandleForAnalysis(),"empty Copy");
    }
}
int main(int argc,char** argv)
{
    if(argc==11 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[9]{};for(unsigned i=0;i<9;++i)v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),unsigned(v[1]),std::uint32_t(v[2]),v[3],v[4],v[5]!=0,std::uint32_t(v[6]),v[7],v[8])<<'\n';
        return EXIT_SUCCESS;
    }
    CheckFactoryAndClone<wxGlyphState>(37);CheckFactoryAndClone<wxSpiritAwayState>(0);
    for(unsigned classification: {15u,34u,44u,46u})
    {
        const auto variant=classification==15?0u:classification==34?0x800000u:classification==44?0x1000000u:0x1800000u;
        Require((wxGlyphState::ComposeKeyForAnalysis(0xFFFFFFFF,classification)&0xF800000u)==variant,"native variant table");
    }
    wxGlyphState glyph;wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(glyph,request);
    glyph.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);glyph.SetPendingHandleForAnalysis(Pointer(11));glyph.vfunc_30(request);
    Require(glyph.GetPendingHandleForAnalysis()==Pointer(22) && host.word==0 && host.trace.find("stop:")==std::string::npos,
        "changed handle queues without releasing old pending");
    host.trace.clear();glyph.vfunc_30(request);Require(host.trace.find("start:")==std::string::npos
        && host.trace.find("word@")!=std::string::npos,"same handle skips playback but still clears control");
    host.first=host.second=22;Require(glyph.vfunc_34(0) && !host.first && !host.second && !glyph.vfunc_34(0),"consuming permission");
    wxSpiritAwayState spirit;wxAnimationRequestForAnalysis unchanged{0xFFFFFFFF};
    Require(spirit.vfunc_1C(unchanged) && spirit.vfunc_20(unchanged) && spirit.vfunc_34(99)
        && !spirit.vfunc_38(99) && unchanged.packedKey==0xFFFFFFFF,"SpiritAway exactly inherits base behavior");
    std::cout<<"Glyph and SpiritAway state checks passed\n";
}
