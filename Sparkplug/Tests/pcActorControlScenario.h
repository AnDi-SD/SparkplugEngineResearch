#pragma once
#include "Code/Sparkplug/spActor.h"
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <array>
namespace sparkplug::tests
{
    inline void ActorControlCheck(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
    inline std::uint32_t ActorControlBits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    inline std::array<std::uint32_t,20> ActorControlWords(const reconstruction::spActor::PlaybackStateForAnalysis& s)
    {
        return {s.mode,s.reverse,ActorControlBits(s.weight),s.fadeMode,ActorControlBits(s.fadeInRate),ActorControlBits(s.fadeOutRate),
            ActorControlBits(s.transitionDuration),s.hasLoopCallback,static_cast<std::uint32_t>(s.callbackCookie),ActorControlBits(s.timeMultiplier),ActorControlBits(s.sampleTime),
            s.stopAfterFade,s.status,s.slotIndex,s.bindingUseCount,s.running,s.priority,ActorControlBits(s.normalizedProgress),ActorControlBits(s.fadeThreshold),ActorControlBits(s.elapsedTime)};
    }
    inline std::string ActorControlScenario(const std::string& mode)
    {
        using namespace reconstruction;using A=spActor;
        struct Restore{std::size_t value=A::GetDefaultPlaybackCapacityForAnalysis();~Restore(){(void)A::SetDefaultPlaybackCapacityForAnalysis(value);}} restore;
        spAnimationManager manager;
        const bool capacity=mode.rfind("capacity",0)==0,reset=mode.rfind("reset",0)==0;
        unsigned count=capacity?std::stoul(mode.substr(8)):reset?40:2;
        ActorControlCheck(A::SetDefaultPlaybackCapacityForAnalysis(count),"bounded default capacity");A actor;
        ActorControlCheck(actor.GetPlaybackCapacityForAnalysis()==count,"factory configured capacity");
        if(reset){actor.GetPlaybackForAnalysis(0)->weight=.625f;count=std::stoul(mode.substr(5));ActorControlCheck(actor.ResetPlaybackCapacityForAnalysis(count),"actual empty state replacement");}
        std::array<std::unique_ptr<spAnimation>,3> animations;
        if(!capacity&&!reset)
        {
            for(auto& animation:animations)animation=std::make_unique<spAnimation>();
            for(unsigned i=0;i<2;++i)
            {
                auto& s=*actor.GetPlaybackForAnalysis(i);s.animation=animations[mode=="fade-duplicate"?0:i].get();
                s.mode=1;s.weight=.625f;s.fadeMode=4;s.fadeOutRate=.75f;s.timeMultiplier=1;s.sampleTime=.125f;s.status=1;
                s.bindingUseCount=mode=="fade-duplicate"?i+1:i==0;s.running=true;s.fadeThreshold=.75f;s.elapsedTime=.25f;
            }
            if(mode=="fade-inactive"){actor.GetPlaybackForAnalysis(0)->bindingUseCount=0;actor.GetPlaybackForAnalysis(0)->running=false;}
            if(mode=="queries-counter")actor.GetPlaybackForAnalysis(0)->running=false;
            else
            {
                const float duration=mode=="fade-zero"?0.f:mode=="fade-negative"?-2.f:mode=="fade-third"?3.f:2.f;
                ActorControlCheck(actor.FadeOutAndStopForAnalysis(animations[mode=="fade-missing"?2:0].get(),duration,-.375f),"actual fade-stop control");
            }
        }
        std::ostringstream out;out<<"[\""<<mode<<"\","<<count<<",[";
        for(unsigned i=0;i<count;++i)
        {
            const auto& s=*actor.GetPlaybackForAnalysis(i);int animation=-1;
            for(int j=0;j<3;++j)if(animations[j]&&animations[j].get()==s.animation)animation=j;
            out<<(i?",[":"[")<<animation;
            for(auto word:ActorControlWords(s))out<<','<<word;out<<']';
        }
        out<<"],[";bool comma=false;
        const auto query=[&](const spAnimation* target)
        {
            const auto* state=actor.FindPlaybackForAnalysis(target);int index=-1;
            for(unsigned i=0;i<count;++i)if(state==actor.GetPlaybackForAnalysis(i))index=static_cast<int>(i);
            out<<(comma?",[":"[")<<index<<','<<actor.HasUsedAnimationForAnalysis(target)<<']';comma=true;
        };
        if(animations[0])for(const auto& animation:animations)query(animation.get());query(nullptr);
        out<<"]]";return out.str();
    }
}
