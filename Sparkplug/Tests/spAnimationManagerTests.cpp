#include "Code/Sparkplug/spAnimationManager.h"
#include "Code/Sparkplug/spActor.h"
#include "Analysis/PC/SparkplugAbi.h"
#include <functional>
#include <iostream>
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
    class CallbackController final : public spController
    {
      public:
        std::function<void(float)> callback;
        void ApplyForAnalysis(float delta) override
        {
            if (callback)
                callback(delta);
        }
    };
    class OtherEvaluator final : public spTransformEval
    {
      public:
        SampleForAnalysis EvaluateForAnalysis(float) override
        {
            return {};
        }
    };
    void Names()
    {
        spAnimationManager manager;
        Check(spAnimationManager::GetInstance() == &manager, "manager singleton");
        Check(manager.IsKindOf(spBaseObject::ClassID) && manager.IsExactly(0x5D214CC1),
              "original manager RTTI");
        Check(manager.GetFrameForAnalysis() == 1 && manager.GetNextSlotForAnalysis() == 1,
              "original counters start at one");
        Check(manager.GetControllerCountForAnalysis() == 0 &&
                  manager.GetNameCountForAnalysis() == 0,
              "empty defaults");
        Check(manager.BindNameForAnalysis("Head") == 1, "first name ID");
        Check(manager.BindNameForAnalysis("Head") == 1, "duplicate returns same ID");
        Check(manager.FindNameForAnalysis("Head")->references == 2, "shared references");
        Check(manager.BindNameForAnalysis("head") == 2, "case-sensitive key");
        Check(manager.BindNameForAnalysis("") == 3, "empty C string is valid");
        Check(manager.BindNameForAnalysis(std::string(80, 'A')) == 4, "heap-length key");
        manager.UnbindNameForAnalysis("not-found");
        Check(manager.GetNameCountForAnalysis() == 4, "missing unbind no-op");
        manager.UnbindNameForAnalysis("Head");
        Check(manager.FindNameForAnalysis("Head")->references == 1, "decrement shared ref");
        manager.UnbindNameForAnalysis("Head");
        Check(!manager.FindNameForAnalysis("Head"), "last reference removes name");
        Check(manager.BindNameForAnalysis("Head") == 5, "erased ID not recycled");
        Check(manager.BindNameForAnalysis(std::string(4096, 'A')) == -1, "host string bound");
        Check(manager.BindNameForAnalysis(std::string("a\0b", 3)) == -1, "host embedded NUL guard");
        Check(manager.GetNextSlotForAnalysis() == 6, "rejected names do not consume IDs");
        spTransformTrackEval evaluator;
        Check(manager.AttachEvaluatorForAnalysis(evaluator, "Head") == 5,
              "attach uses shared registry");
        Check(evaluator.GetBoundSlotForAnalysis() == 5, "evaluator stores name slot");
        manager.DetachEvaluatorForAnalysis("Head");
        Check(evaluator.GetBoundSlotForAnalysis() == 5 &&
                  manager.FindNameForAnalysis("Head")->references == 1,
              "detach does not reset evaluator slot");
        OtherEvaluator other;
        Check(manager.AttachEvaluatorForAnalysis(other, "Ignored") == 0 &&
                  !manager.FindNameForAnalysis("Ignored"),
              "nontrack evaluator type rejected");
        auto copy = manager.Clone();
        auto* clone = dynamic_cast<spAnimationManager*>(copy.get());
        Check(clone && clone->GetFrameForAnalysis() == 1 && clone->GetNextSlotForAnalysis() == 1 &&
                  clone->GetNameCountForAnalysis() == 0 &&
                  clone->GetControllerCountForAnalysis() == 0,
              "blank native clone semantics");
        Check(spAnimationManager::GetInstance() == clone, "clone singleton replacement");
        copy.reset();
        Check(!spAnimationManager::GetInstance(), "destructor clears singleton");
    }
    void NameLeaseLifetime()
    {
        auto manager = std::make_unique<spAnimationManager>();
        auto first = manager->AcquireNameBindingForAnalysis("Head");
        auto second = manager->AcquireNameBindingForAnalysis("Head");
        Check(first && second && first->GetSlotForAnalysis() == second->GetSlotForAnalysis() &&
                  manager->FindNameForAnalysis("Head")->references == 2,
              "two owned name references");
        auto moved = std::move(*first);
        first.reset();
        Check(moved.BelongsToForAnalysis(*manager) &&
                  manager->FindNameForAnalysis("Head")->references == 2,
              "move transfers reference without unbinding moved-from lease");
        moved = std::move(moved);
        Check(manager->FindNameForAnalysis("Head")->references == 2, "self move keeps reference");
        second.reset();
        Check(manager->FindNameForAnalysis("Head")->references == 1,
              "first lease destruction retains other");
        auto other = manager->AcquireNameBindingForAnalysis("Back");
        moved = std::move(*other);
        Check(!manager->FindNameForAnalysis("Head") &&
                  manager->FindNameForAnalysis("Back")->references == 1,
              "move assignment releases replaced reference exactly once");
        moved.ResetForAnalysis();
        moved.ResetForAnalysis();
        Check(manager->GetNameCountForAnalysis() == 0, "explicit reset is host-idempotent");
        Check(!manager->AcquireNameBindingForAnalysis(std::string(4096, 'A')) &&
                  !manager->AcquireNameBindingForAnalysis(std::string("x\0y", 3)),
              "lease name bounds");
        auto stale = manager->AcquireNameBindingForAnalysis("stale");
        manager.reset();
        spAnimationManager replacement;
        auto current = replacement.AcquireNameBindingForAnalysis("stale");
        Check(!stale->BelongsToForAnalysis(replacement),
              "lease identity does not follow singleton replacement");
        stale.reset();
        Check(replacement.FindNameForAnalysis("stale")->references == 1,
              "dead manager lease cannot release replacement manager reference");
        spAnimation animation;
        auto* track = animation.AppendTrackForAnalysis();
        Check(!track->BindNameForAnalysis(replacement), "unnamed track cannot acquire name lease");
        track->SetName("Head");
        Check(track->BindNameForAnalysis(replacement) && track->IsBoundToForAnalysis(replacement),
              "track owns registry reference");
        const auto slot = track->GetBindingSlotForAnalysis();
        Check(track->BindNameForAnalysis(replacement) &&
                  track->GetBindingSlotForAnalysis() == slot &&
                  replacement.FindNameForAnalysis("Head")->references == 1,
              "repeat binding balances reference");
        auto clone = track->Clone();
        Check(!dynamic_cast<spAnimTrack*>(clone.get())->IsBoundToForAnalysis(replacement) &&
                  replacement.FindNameForAnalysis("Head")->references == 1,
              "name-only clone acquires no lease");
        track->SetName("Back");
        Check(!track->IsBoundToForAnalysis(replacement), "renamed track is not silently re-bound");
        Check(track->BindNameForAnalysis(replacement) && !replacement.FindNameForAnalysis("Head") &&
                  replacement.FindNameForAnalysis("Back")->references == 1,
              "explicit rebind releases original acquired name");
        track->ReleaseKeysForAnalysis();
        Check(replacement.FindNameForAnalysis("Back")->references == 1,
              "release keys does not release name slot");
        track->SetBindingSlotForAnalysis(42);
        Check(!replacement.FindNameForAnalysis("Back") && track->GetBindingSlotForAnalysis() == 42,
              "manual borrowed slot replaces owned lease");
        track->SetName("");
        Check(track->BindNameForAnalysis(replacement), "owned empty track name is valid");
        Check(animation.ResizeTrackCapacityForAnalysis(0) && !replacement.FindNameForAnalysis(""),
              "shrinking constructed track releases its name reference");
    }
    void Controllers()
    {
        spAnimationManager manager;
        std::vector<std::pair<int, float>> calls;
        auto first = std::make_unique<CallbackController>();
        auto middle = std::make_unique<CallbackController>();
        auto last = std::make_unique<CallbackController>();
        first->callback = [&](float delta) { calls.emplace_back(1, delta); };
        middle->callback = [&](float delta) { calls.emplace_back(2, delta); };
        last->callback = [&](float delta) { calls.emplace_back(3, delta); };
        Check(manager.GetControllerCountForAnalysis() == 3,
              "controllers auto-register in constructor order");
        Check(!manager.RegisterControllerForAnalysis(*first), "host duplicate registration guard");
        middle->SetEnabledForAnalysis(false);
        Check(manager.AdvanceFrameForAnalysis(.25f), "bounded manager frame");
        Check(calls == std::vector<std::pair<int, float>>{{1, .25f}, {3, .25f}},
              "native head-to-next order and enabled gate");
        middle.reset();
        Check(manager.GetControllerCountForAnalysis() == 2, "middle destruction unlinks");
        first.reset();
        Check(manager.GetControllerCountForAnalysis() == 1, "head destruction unlinks");
        calls.clear();
        Check(manager.AdvanceFrameForAnalysis(.5f) &&
                  calls == std::vector<std::pair<int, float>>{{3, .5f}},
              "remaining tail is new head");
        last.reset();
        Check(manager.GetControllerCountForAnalysis() == 0, "last destruction empties list");
        Check(manager.AdvanceFrameForAnalysis(1) && manager.GetFrameForAnalysis() == 4,
              "empty list still advances frame counter");
        Check(!manager.AdvanceFrameForAnalysis(std::numeric_limits<float>::infinity()) &&
                  manager.GetFrameForAnalysis() == 4,
              "host nonfinite guard is before mutation");
    }
    void PostCallNext()
    {
        spAnimationManager manager;
        CallbackController first;
        std::unique_ptr<CallbackController> appended;
        std::vector<int> calls;
        first.callback = [&](float delta) {
            calls.push_back(1);
            Check(!manager.AdvanceFrameForAnalysis(delta), "host recursive frame guard");
            appended = std::make_unique<CallbackController>();
            appended->callback = [&](float) { calls.push_back(2); };
        };
        Check(manager.AdvanceFrameForAnalysis(.25f) && calls == std::vector<int>{1, 2},
              "post-callback next read observes a new tail in same frame");
        first.callback = [](float) { throw std::runtime_error("fixture failure"); };
        bool threw = false;
        try
        {
            (void)manager.AdvanceFrameForAnalysis(1);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        first.callback = {};
        Check(threw && manager.AdvanceFrameForAnalysis(1),
              "host frame guard resets after callback exception");
    }
    void ActorChain()
    {
        spAnimationManager manager;
        spActor actor;
        spAnimation animation;
        Check(animation.SetTotalTimeForAnalysis(4), "actor animation duration");
        auto& state = *actor.GetPlaybackForAnalysis(0);
        state.animation = &animation;
        state.mode = 1;
        state.weight = state.timeMultiplier = 1;
        state.bindingUseCount = 1;
        state.running = true;
        Check(manager.AdvanceFrameForAnalysis(1) && state.sampleTime == 1,
              "manager calls real reconstructed actor tick");
        actor.SetEnabledForAnalysis(false);
        Check(manager.AdvanceFrameForAnalysis(1) && state.sampleTime == 1,
              "controller enabled flag gates the entire actor tick");
        actor.SetEnabledForAnalysis(true);
        actor.SetAppliesTransformsForAnalysis(false);
        Check(manager.AdvanceFrameForAnalysis(1) && state.sampleTime == 2,
              "actor applies flag is separate from manager enable gate");
        actor.SetAdvancesWhileDisabledForAnalysis(false);
        Check(manager.AdvanceFrameForAnalysis(1) && state.sampleTime == 2,
              "both actor flags off suppress tick work");
        actor.SetEnabledForAnalysis(false);
        actor.SetTimeMultiplierForAnalysis(7);
        auto cloned = actor.Clone();
        auto* clone = dynamic_cast<spActor*>(cloned.get());
        Check(clone && !clone->IsEnabledForAnalysis() &&
                  clone->GetPlaybackCapacityForAnalysis() == 40 &&
                  clone->GetPlaybackForAnalysis(0)->animation == nullptr &&
                  clone->GetPlaybackForAnalysis(0)->sampleTime == 0,
              "actor clone copies enabled only, not playback states");
        Check(manager.GetControllerCountForAnalysis() == 2,
              "actor clone auto-registers independently");
        clone->SetEnabledForAnalysis(true);
        auto& cloneState = *clone->GetPlaybackForAnalysis(0);
        cloneState.animation = &animation;
        cloneState.mode = 1;
        cloneState.timeMultiplier = cloneState.weight = 1;
        cloneState.running = true;
        cloneState.bindingUseCount = 1;
        Check(manager.AdvanceFrameForAnalysis(1) && cloneState.sampleTime == 1,
              "cloned actor has default speed and advance flags, not source configuration");
        cloned.reset();
        Check(manager.GetControllerCountForAnalysis() == 1, "cloned actor unregisters normally");
    }
    void HostLifetimeSafety()
    {
        auto manager = std::make_unique<spAnimationManager>();
        auto controller = std::make_unique<CallbackController>();
        manager.reset();
        controller.reset(); // native requires reverse order; host detaches safely
        CallbackController standalone;
        Check(!spAnimationManager::GetInstance(),
              "host standalone controller creates no implicit singleton");
        spAnimationManager another;
        Check(another.RegisterControllerForAnalysis(standalone),
              "explicit registration of standalone controller");
        another.UnregisterControllerForAnalysis(standalone);
        Check(another.GetControllerCountForAnalysis() == 0, "explicit safe unregister");
    }
} // namespace
int main()
{
    try
    {
        Names();
        NameLeaseLifetime();
        Controllers();
        PostCallNext();
        ActorChain();
        HostLifetimeSafety();
        std::cout << "PASS " << checks << '/' << checks << ": animation manager checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
