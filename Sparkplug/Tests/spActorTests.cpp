#include "Code/Sparkplug/spActor.h"
#include "Code/Sparkplug/spNodeController.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    void Prepare(spActor& actor, spAnimation& animation)
    {
        Check(animation.SetTotalTimeForAnalysis(4), "animation total time");
        for (std::uint32_t i = 0; i < 5; ++i)
            Check(animation.InsertTagForAnalysis({"tag", static_cast<float>(i), i}), "sorted tag");
        auto& state = *actor.GetPlaybackForAnalysis(0);
        state.animation = &animation;
        state.mode = 1;
        state.weight = 1;
        state.timeMultiplier = 1;
        state.bindingUseCount = 1;
        state.running = true;
        Check(actor.SetControllerBindingsForAnalysis({{0, {}, {}}}),
              "explicit first-input controller seam");
    }
    void Print(const spActor::PlaybackStateForAnalysis& state,
               const std::vector<spActor::ActionForAnalysis>& actions)
    {
        using Kind = spActor::ActionKindForAnalysis;
        std::cout << std::setprecision(9) << "{\"sample\":" << state.sampleTime
                  << ",\"progress\":" << state.normalizedProgress
                  << ",\"elapsed\":" << state.elapsedTime
                  << ",\"transition\":" << state.transitionDuration
                  << ",\"weight\":" << state.weight << ",\"fade\":" << state.fadeMode
                  << ",\"status\":" << state.status << ",\"uses\":" << state.bindingUseCount
                  << ",\"running\":" << state.running << ",\"events\":[";
        bool comma = false;
        for (const auto& action : actions)
        {
            if (comma)
                std::cout << ',';
            comma = true;
            switch (action.kind)
            {
            case Kind::Event:
                std::cout << "[\"event\"," << action.eventCode << ',';
                if (action.tagOrdinal)
                    std::cout << *action.tagOrdinal;
                else
                    std::cout << "\"animation\"";
                std::cout << ']';
                break;
            case Kind::LoopCallback:
                std::cout << "[\"callback\"]";
                break;
            case Kind::ControllerLookup:
                std::cout << "[\"get-evaluator\"]";
                break;
            case Kind::Direct:
                std::cout << "[\"direct\"," << action.time << ']';
                break;
            case Kind::Blend:
                std::cout << "[\"blend\"," << action.time << ',' << action.factor << ']';
                break;
            case Kind::Rebind:
                std::cout << "[\"rebind\"]";
                break;
            case Kind::Flush:
                std::cout << "[\"flush\"]";
                break;
            }
        }
        std::cout << "]}\n";
    }
    void Batch()
    {
        int count = 0;
        if (!(std::cin >> count) || count < 1 || count > 128)
            throw std::runtime_error("Bounded case count");
        for (int i = 0; i < count; ++i)
        {
            spActor actor;
            spAnimation animation;
            Prepare(actor, animation);
            auto& state = *actor.GetPlaybackForAnalysis(0);
            float delta = 0, actorMultiplier = 1;
            bool applies = true, advances = true;
            if (!(std::cin >> delta >> state.mode >> state.reverse >> state.normalizedProgress >>
                  state.sampleTime >> state.weight >> state.fadeMode >> state.fadeInRate >>
                  state.fadeOutRate >> state.fadeThreshold >> state.elapsedTime >>
                  state.transitionDuration >> applies >> advances >> state.running >>
                  state.bindingUseCount >> state.timeMultiplier >> actorMultiplier >>
                  state.stopAfterFade >> state.hasLoopCallback))
                throw std::runtime_error("Invalid probe input");
            actor.SetAppliesTransformsForAnalysis(applies);
            actor.SetAdvancesWhileDisabledForAnalysis(advances);
            actor.SetTimeMultiplierForAnalysis(actorMultiplier);
            std::vector<spActor::ActionForAnalysis> actions;
            std::string error;
            if (!actor.TickForAnalysis(delta, actions, &error))
                throw std::runtime_error(error);
            Print(state, actions);
        }
    }
    void Tests()
    {
        spActor actor;
        Check(actor.IsKindOf(spController::ClassID) && actor.IsKindOf(spSubController::ClassID),
              "original actor RTTI chain");
        Check(actor.IsEnabledForAnalysis(), "controller enabled default");
        Check(actor.GetPlaybackCapacityForAnalysis() == 40, "native initial capacity");
        for (std::size_t i = 0; i < 40; ++i)
        {
            const auto& state = *actor.GetPlaybackForAnalysis(i);
            Check(state.slotIndex == i && !state.animation && !state.running &&
                      !state.bindingUseCount && state.timeMultiplier == 0 && state.weight == 0,
                  "post-memset default state plus index");
        }
        Check(!actor.GetPlaybackForAnalysis(40), "bounded playback lookup");
        Check(dynamic_cast<spActor*>(actor.Clone().get()) != nullptr, "original blank actor clone");
        spAnimation animation;
        Prepare(actor, animation);
        auto& state = *actor.GetPlaybackForAnalysis(0);
        std::vector<spActor::ActionForAnalysis> actions;
        std::string error;
        state.mode = 0;
        Check(actor.TickForAnalysis(4, actions) && state.running && state.sampleTime == 4,
              "once endpoint remains active at equality");
        Check(actor.TickForAnalysis(.25f, actions) && !state.running && state.bindingUseCount == 0,
              "once finishes after endpoint");
        state.running = true;
        state.bindingUseCount = 1;
        state.normalizedProgress = 0;
        state.elapsedTime = 0;
        Check(animation.SetTotalTimeForAnalysis(0), "prepare invalid active duration");
        Check(!actor.TickForAnalysis(1, actions, &error) && state.elapsedTime == 0 &&
                  !error.empty(),
              "host invalid duration preflight");
        Check(animation.SetTotalTimeForAnalysis(4), "restore duration");
        Check(!actor.TickForAnalysis(std::numeric_limits<float>::infinity(), actions),
              "nonfinite step rejected");
        Check(!actor.TickForAnalysis(100000, actions), "bounded loop traversal");

        auto* track = animation.AppendTrackForAnalysis();
        spAnimTrack::TrackDataForAnalysis keys{};
        sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis positions;
        positions.representation = 1;
        positions.times = {0, 1, 2};
        positions.values = {0, 0, 0, 10, 20, 30, 20, 40, 60};
        keys[0][0] = positions;
        Check(track->SetKeysForAnalysis(std::move(keys)), "prepared original track keys");
        auto sampler = track->GetSamplerForAnalysis();
        auto node = std::make_shared<spNode>();
        spNodeController controller;
        controller.SetNodeForAnalysis(node);
        auto* evaluator = dynamic_cast<spTransformTrackEval*>(controller.GetEvaluatorForAnalysis());
        Check(evaluator != nullptr, "native default controller evaluator");
        Check(evaluator->SetInputsForAnalysis({{&state.evaluation, &sampler, 0, {}}}),
              "actor evaluation view bound");
        Check(actor.SetControllerBindingsForAnalysis(
                  {{0, [&](float t) { controller.ApplyForAnalysis(t); },
                    [&](float t, float factor) { controller.BlendForAnalysis(t, factor); }}}),
              "real node-controller callbacks");
        state.mode = 1;
        state.normalizedProgress = 0;
        state.sampleTime = 0;
        state.elapsedTime = 0;
        state.weight = 1;
        state.timeMultiplier = 1;
        Check(actor.TickForAnalysis(1, actions), "actor to real keys/evaluator/controller tick");
        Check(node->GetPositionForAnalysis()[0] == 10 && node->GetPositionForAnalysis()[1] == 20,
              "node receives original PRS sample");
        Check(node->UpdateWorldForAnalysis() && node->GetWorldPositionForAnalysis()[2] == 30,
              "actor-to-world chain");
        std::cout << "PASS " << checks << '/' << checks << ": actor scheduler tests\n";
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--probe-batch")
            Batch();
        else if (argc == 1)
            Tests();
        else
            throw std::runtime_error("Unexpected actor test arguments");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
