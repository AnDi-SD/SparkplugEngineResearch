#pragma once
#include "pcActorControlScenario.h"
#include "pcSanFixture.h"
namespace sparkplug::tests
{
    inline std::string ActorControlPipeline(const char* asset,const std::string& mode)
    {
        using namespace reconstruction;using A=spActor;using Kind=A::ActionKindForAnalysis;
        struct Restore{std::size_t value=A::GetDefaultPlaybackCapacityForAnalysis();~Restore(){(void)A::SetDefaultPlaybackCapacityForAnalysis(value);}} restore;
        ActorControlCheck(A::SetDefaultPlaybackCapacityForAnalysis(2),"observed configured capacity");
        spAnimationManager manager;std::array<std::unique_ptr<spAnimation>,3> animations;
        for(auto& animation:animations)animation=ReadOwnedSan(asset,manager);
        A actor;std::array<std::shared_ptr<spNode>,2> nodes{std::make_shared<spNode>(),std::make_shared<spNode>()};std::string error;
        for(unsigned i=0;i<2;++i){nodes[i]->SetName(animations[0]->GetTrackForAnalysis(i)->GetName());ActorControlCheck(actor.DiscoverNodeForAnalysis(nodes[i],&error),error.c_str());}
        std::vector<A::ActionForAnalysis> actions;float requestWeight=0;std::ostringstream out;out<<"[\""<<mode<<"\",[";bool comma=false;
        const auto capture=[&](const std::string& label,std::uint32_t result=0xffffffff)
        {
            out<<(comma?",[\"":"[\"")<<label<<"\","<<result<<','<<ActorControlBits(requestWeight)<<','<<manager.GetFrameForAnalysis()<<",[";comma=true;
            for(unsigned i=0;i<2;++i)
            {
                const auto& s=*actor.GetPlaybackForAnalysis(i);int animation=-1;
                for(int j=0;j<3;++j)if(s.animation==animations[j].get())animation=j;
                out<<(i?",[":"[")<<animation;for(auto word:ActorControlWords(s))out<<','<<word;out<<']';
            }
            out<<"],[";
            for(unsigned i=0;i<2;++i)
            {
                const auto* e=static_cast<spTransformTrackEval*>(actor.GetOwnedControllerForAnalysis(i)->GetEvaluatorForAnalysis());
                out<<(i?",[":"[")<<e->GetBoundSlotForAnalysis()<<','<<e->GetInputCountForAnalysis();
                for(const auto& input:e->GetPhysicalInputsForAnalysis())
                {
                    int state=-1;for(unsigned j=0;j<2;++j)if(input.playback==&actor.GetPlaybackForAnalysis(j)->evaluation)state=j;
                    out<<",["<<state<<','<<bool(input.sampler)<<','<<input.priority;
                    for(const auto& cache:{input.cache.position,input.cache.rotation,input.cache.scale})for(auto word:cache)out<<','<<word;
                    out<<']';
                }
                out<<']';
            }
            out<<"],[";bool actionComma=false;
            for(const auto& action:actions)
            {
                if(action.kind!=Kind::Event&&action.kind!=Kind::ImmediateEvent&&action.kind!=Kind::Flush)continue;
                out<<(actionComma?",[":"[");actionComma=true;
                if(action.kind==Kind::Flush)out<<2;
                else out<<(action.kind==Kind::ImmediateEvent?1:0)<<','<<action.eventCode<<','<<action.playbackIndex<<','<<(action.payload==A::EventPayloadForAnalysis::Playback);
                out<<']';
            }
            out<<"]]";
        };
        const auto start=[&](unsigned index,unsigned fade,const char* label)
        {
            A::StartRequestForAnalysis request;request.animation=animations[index].get();request.fadeMode=fade;request.weight=.25f;
            request.fallbackFadeInRate=.5f;request.fallbackFadeOutRate=.75f;
            const auto result=actor.StartForAnalysis(request,actions,&error);ActorControlCheck(error.empty(),error.c_str());
            requestWeight=request.weight;capture(label,result?static_cast<std::uint32_t>(*result):0xffffffff);
        };
        start(0,mode=="no-free"||mode=="restart-full"?2:0,"start0");
        if(mode=="no-free"||mode=="restart-full")
        {start(1,2,"start1");start(mode=="no-free"?2:1,0,"request");}
        else
        {
            actions.clear();ActorControlCheck(actor.FadeOutAndStopForAnalysis(animations[0].get(),mode=="fade-negative-rate"?0.f:.5f,mode=="fade-negative-rate"?-.5f:0.f),"deferred fade-stop");capture("fade");
            const std::vector<float> deltas=mode=="fade-at-zero"?std::vector<float>{0,.25f}:mode=="fade-negative-rate"?std::vector<float>{.25f}:std::vector<float>{.25f,.25f,.125f};
            for(unsigned i=0;i<deltas.size();++i)
            {
                // Same established observation contract as pcActorScenario:
                // advance manager counter while gated, then expose Tick actions.
                // Actual source manager dispatch is independently covered CP66.
                actor.SetEnabledForAnalysis(false);ActorControlCheck(manager.AdvanceFrameForAnalysis(deltas[i]),"manager counter");actor.SetEnabledForAnalysis(true);
                ActorControlCheck(actor.TickForAnalysis(deltas[i],actions,&error),error.c_str());capture("tick"+std::to_string(i));
            }
        }
        out<<"]]";return out.str();
    }
}
