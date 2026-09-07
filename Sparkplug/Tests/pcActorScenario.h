#pragma once

#include "pcSanFixture.h"
#include "Code/Sparkplug/spActor.h"
#include <iomanip>
#include <iostream>

namespace sparkplug::tests
{
    inline int ActorScenario(const char* asset, const std::string& scenario)
    {
        using namespace reconstruction;
        using Kind = spActor::ActionKindForAnalysis;
        spAnimationManager manager;
        std::array<std::unique_ptr<spAnimation>, 3> animations;
        for (auto& animation : animations)
            animation = ReadOwnedSan(asset, manager);
        std::array<std::shared_ptr<spNode>, 2> nodes{std::make_shared<spNode>(),
                                                     std::make_shared<spNode>()};
        spActor actor;
        std::string error;
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            nodes[i]->SetName(animations[0]->GetTrackForAnalysis(i)->GetName());
            if (!actor.DiscoverNodeForAnalysis(nodes[i], &error))
                throw std::runtime_error(error);
        }
        std::vector<spActor::ActionForAnalysis> actions;
        const auto stateIndex = [&](const spTransformTrackEval::PlaybackForAnalysis* playback) {
            for (int i = 0; i < 40; ++i)
                if (playback == &actor.GetPlaybackForAnalysis(i)->evaluation)
                    return i;
            return -1;
        };
        bool comma = false;
        std::cout << std::setprecision(9) << '[';
        const auto snapshot = [&] {
            std::cout << (comma ? ",{" : "{") << "\"frame\":" << manager.GetFrameForAnalysis()
                      << ",\"states\":[";
            comma = true;
            for (int i = 0; i < 3; ++i)
            {
                const auto& s = *actor.GetPlaybackForAnalysis(i);
                int animationIndex = -1;
                for (int a = 0; a < 3; ++a)
                    if (s.animation == animations[a].get())
                        animationIndex = a;
                std::cout << (i ? ",[" : "[") << animationIndex << ',' << s.mode << ',' << s.reverse
                          << ',' << s.weight << ',' << s.fadeMode << ',' << s.fadeInRate << ','
                          << s.fadeOutRate << ',' << s.transitionDuration << ','
                          << s.hasLoopCallback << ',' << s.callbackCookie << ',' << s.timeMultiplier
                          << ',' << s.sampleTime << ',' << s.stopAfterFade << ',' << s.status << ','
                          << s.slotIndex << ',' << s.bindingUseCount << ',' << s.running << ','
                          << s.priority << ',' << s.normalizedProgress << ',' << s.fadeThreshold
                          << ',' << s.elapsedTime << ']';
            }
            std::cout << "],\"inputs\":[";
            for (std::size_t n = 0; n < nodes.size(); ++n)
            {
                const auto* eval = static_cast<spTransformTrackEval*>(
                    actor.GetOwnedControllerForAnalysis(n)->GetEvaluatorForAnalysis());
                std::cout << (n ? ",[" : "[") << eval->GetBoundSlotForAnalysis() << ','
                          << eval->GetInputCountForAnalysis();
                for (const auto& input : eval->GetPhysicalInputsForAnalysis())
                {
                    std::cout << ",[" << stateIndex(input.playback) << ',' << bool(input.sampler)
                              << ',' << input.priority;
                    for (const auto& cache :
                         {input.cache.position, input.cache.rotation, input.cache.scale})
                        for (auto value : cache)
                            std::cout << ',' << value;
                    std::cout << ']';
                }
                std::cout << ']';
            }
            std::cout << "],\"nodes\":[";
            for (std::size_t n = 0; n < nodes.size(); ++n)
            {
                if (!nodes[n]->UpdateWorldForAnalysis(1))
                    throw std::runtime_error("Fixture world update failed");
                std::cout << (n ? ",[" : "[");
                bool valueComma = false;
                const auto print = [&](const auto& values) {
                    for (float v : values)
                    {
                        std::cout << (valueComma ? "," : "") << v;
                        valueComma = true;
                    }
                };
                print(nodes[n]->GetPositionForAnalysis());
                print(nodes[n]->GetScaleForAnalysis());
                print(nodes[n]->GetOrientationForAnalysis());
                print(nodes[n]->GetWorldPositionForAnalysis());
                print(nodes[n]->GetWorldScaleForAnalysis());
                print(nodes[n]->GetWorldOrientationForAnalysis());
                std::cout << ',' << nodes[n]->GetFlagsForAnalysis() << ']';
            }
            std::cout << "],\"events\":[";
            bool eventComma = false;
            for (const auto& action : actions)
                if (action.kind == Kind::Event || action.kind == Kind::ImmediateEvent ||
                    action.kind == Kind::Flush)
                {
                    std::cout << (eventComma ? ",[" : "[");
                    eventComma = true;
                    if (action.kind == Kind::Flush)
                        std::cout << "2]";
                    else
                        std::cout << (action.kind == Kind::Event ? 0 : 1) << ',' << action.eventCode
                                  << ',' << action.playbackIndex << ','
                                  << static_cast<int>(action.payload) << ','
                                  << (action.tagOrdinal ? int(*action.tagOrdinal) : -1) << ']';
                }
            std::cout << "]}";
        };
        const auto start = [&](int index, std::uint32_t fade, std::uint32_t mode = 1,
                               float transition = 0, float outDuration = 0) {
            spActor::StartRequestForAnalysis request;
            request.animation = animations[index].get();
            request.mode = mode;
            request.fadeMode = fade;
            request.weight = .25f;
            request.fallbackFadeInRate = .5f;
            request.fallbackFadeOutRate = .75f;
            request.transitionDuration = transition;
            request.fadeOutDuration = outDuration;
            if (!actor.StartForAnalysis(request, actions, &error))
                throw std::runtime_error(error);
            snapshot();
        };
        const auto tick = [&](float delta) {
            // The manager is already tested to snapshot delta and gate the list;
            // capture actor actions explicitly while dispatching it through manager.
            actor.SetEnabledForAnalysis(false);
            if (!manager.AdvanceFrameForAnalysis(delta))
                throw std::runtime_error("Manager fixture frame");
            actor.SetEnabledForAnalysis(true);
            if (!actor.TickForAnalysis(delta, actions, &error))
                throw std::runtime_error(error);
            snapshot();
        };
        const auto stop = [&](int index, bool suppress) {
            if (!actor.StopForAnalysis(animations[index].get(), suppress, actions, &error))
                throw std::runtime_error(error);
            snapshot();
        };
        if (scenario == "normal")
        {
            start(0, 0);
            tick(.25f);
            tick(.75f);
            tick(.125f);
            stop(0, false);
        }
        else if (scenario == "blend")
        {
            start(0, 2);
            tick(.25f);
            start(1, 2);
            tick(.25f);
            start(1, 0);
            tick(.25f);
            if (!actor.StopAllForAnalysis(actions, &error))
                throw std::runtime_error(error);
            snapshot();
        }
        else if (scenario == "oneshot")
        {
            start(0, 0, 0);
            tick(.75f);
            tick(.5f);
            tick(.25f);
        }
        else if (scenario == "transition")
        {
            start(0, 0, 1, .5f);
            tick(.25f);
            tick(.25f);
            tick(.25f);
        }
        else if (scenario == "fade_stop")
        {
            start(0, 3, 1, 0, .5f);
            actor.GetPlaybackForAnalysis(0)->stopAfterFade = true;
            tick(.75f);
            tick(.25f);
        }
        else if (scenario == "suppressed")
        {
            start(0, 2);
            start(1, 2);
            stop(0, true);
            tick(.25f);
            stop(1, true);
        }
        else
            throw std::runtime_error("Unknown bounded actor scenario");
        std::cout << "]\n";
        return 0;
    }
} // namespace sparkplug::tests
