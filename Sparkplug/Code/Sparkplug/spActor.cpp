#include "spActor.h"
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace sparkplug::reconstruction
{
    std::size_t spActor::defaultPlaybackCapacity_=spActor::DefaultPlaybackCapacityForAnalysis;
    bool spActor::SetDefaultPlaybackCapacityForAnalysis(std::size_t capacity) noexcept
    {
        if(capacity>DefaultPlaybackCapacityForAnalysis)return false;
        defaultPlaybackCapacity_=capacity;return true;
    }
    std::size_t spActor::GetDefaultPlaybackCapacityForAnalysis() noexcept{return defaultPlaybackCapacity_;}
    bool spActor::ResetPlaybackCapacityForAnalysis(std::size_t capacity,std::string* error)
    {
        if(error)error->clear();
        if(capacity>DefaultPlaybackCapacityForAnalysis||!controllers_.empty()||!ownedControllers_.empty())
        {if(error)*error="Playback replacement requires bounded capacity and no controller bindings";return false;}
        for(const auto& state:states_)if(state.running||state.bindingUseCount)
        {if(error)*error="Playback replacement would invalidate active state views";return false;}
        std::vector<PlaybackStateForAnalysis> replacement(capacity);
        states_.swap(replacement);
        for(std::size_t i=0;i<states_.size();++i)
        {states_[i].slotIndex=static_cast<std::uint32_t>(i);states_[i].evaluation.bindingUseCount=&states_[i].bindingUseCount;}
        return true;
    }
    const spActor::PlaybackStateForAnalysis* spActor::FindPlaybackForAnalysis(const spAnimation* animation) const noexcept
    {
        for(const auto& state:states_)if(state.animation==animation)return &state;
        return nullptr;
    }
    bool spActor::HasUsedAnimationForAnalysis(const spAnimation* animation) const noexcept
    {
        for(const auto& state:states_)if(state.animation==animation&&state.bindingUseCount)return true;
        return false;
    }
    bool spActor::FadeOutAndStopForAnalysis(const spAnimation* animation,float duration,float fallbackRate,
                                            std::string* error)
    {
        if(error)error->clear();
        // Finite arguments and a nonnull borrowed resource are host policy.
        // Native5A16D0 also matches a null pointer and accepts exceptional floats.
        if(!animation||!std::isfinite(duration)||!std::isfinite(fallbackRate))
        {if(error)*error="Fade request requires a resource and finite rates";return false;}
        for(auto& state:states_)if(state.animation==animation)
        {
            state.fadeOutRate=duration>0?static_cast<float>(1.0/static_cast<double>(duration)):fallbackRate;
            state.fadeMode=3;state.fadeThreshold=0;state.stopAfterFade=true;break;
        }
        return true;
    }
    spActor::spActor() : states_(defaultPlaybackCapacity_)
    {
        // 0x005A1600 constructs defaults THEN memset clears the entire array.
        // Only slot index +0x44 is filled afterwards: preserve the final state.
        for (std::size_t i = 0; i < states_.size(); ++i)
        {
            states_[i].slotIndex = static_cast<std::uint32_t>(i);
            states_[i].evaluation.bindingUseCount = &states_[i].bindingUseCount;
        }
    }
    spActor::~spActor()
    {
        controllers_.clear(); // release host callbacks before their captured pointees
        for (auto& owned : ownedControllers_)
        {
            // Original 005A32F0 vector order: clear2000, unbind, delete controller.
            if (auto* node = owned.controller->GetNodeForAnalysis())
                node->flags_ &= ~0x2000U;
            owned.name.ResetForAnalysis();
            owned.controller.reset();
        }
    }
    std::optional<std::size_t> spActor::PlaybackIndexForAnalysis(
        const spTransformTrackEval::PlaybackForAnalysis* playback) const noexcept
    {
        for (std::size_t i = 0; playback && i < states_.size(); ++i)
            if (&states_[i].evaluation == playback)
                return i;
        return std::nullopt;
    }
    std::size_t spActor::GetOwnedControllerCountForAnalysis() const noexcept
    {
        return ownedControllers_.size();
    }
    spNodeController* spActor::GetOwnedControllerForAnalysis(std::size_t index) noexcept
    {
        return index < ownedControllers_.size() ? ownedControllers_[index].controller.get()
                                                : nullptr;
    }
    bool spActor::DiscoverNodeForAnalysis(std::shared_ptr<spNode> node, std::string* error)
    {
        if (error)
            error->clear();
        const auto fail = [error](const char* text) {
            if (error)
                *error = text;
            return false;
        };
        auto* manager = GetRegisteredManagerForAnalysis();
        if (!manager || !node || (!ownedBindingMode_ && !controllers_.empty()))
            return fail(
                "Discovery requires a live registered manager and no manual controller seam");
        if (!node->GetName() || !node->IsAnimatedForAnalysis() || (node->flags_ & 0x2000U))
        {
            ownedBindingMode_ = true;
            return true; // original skip does not prevent descendant traversal
        }
        if (ownedControllers_.size() >= 4096)
            return fail("Actor node discovery bound exceeded");
        auto name = manager->AcquireNameBindingForAnalysis(node->GetName());
        if (!name)
            return fail("Cannot acquire actor node name binding");
        auto controller = std::make_unique<spNodeController>();
        controller->SetNodeForAnalysis(node);
        auto* evaluator =
            dynamic_cast<spTransformTrackEval*>(controller->GetEvaluatorForAnalysis());
        if (!evaluator)
            return fail("Node controller kind0 did not create track evaluator");
        evaluator->SetBoundSlotForAnalysis(name->GetSlotForAnalysis());
        auto* raw = controller.get();
        ControllerBindingForAnalysis binding{
            {},
            [raw](float time) { raw->ApplyForAnalysis(time); },
            [raw](float time, float factor) { raw->BlendForAnalysis(time, factor); },
            [this, evaluator] { return PlaybackIndexForAnalysis(evaluator->inputs_[0].playback); }};
        ownedControllers_.reserve(ownedControllers_.size() + 1);
        controllers_.reserve(controllers_.size() + 1);
        // Last duplicate name overwrites map value, while both controllers remain owned.
        slotControllers_.insert_or_assign(name->GetSlotForAnalysis(), ownedControllers_.size());
        ownedControllers_.push_back({std::move(controller), std::move(*name)});
        controllers_.push_back(std::move(binding));
        node->flags_ |= 0x2000U;
        ownedBindingMode_ = true;
        return true;
    }
    bool spActor::BindDescendantsForAnalysis(spNode& root, std::string* error)
    {
        if (error)
            error->clear();
        if (root.children_.size() > 4096)
        {
            if (error)
                *error = "Actor root child bound exceeded";
            return false;
        }
        std::vector<std::shared_ptr<spNode>> pending(root.children_.rbegin(),
                                                     root.children_.rend());
        std::vector<std::shared_ptr<spNode>> order;
        std::unordered_set<spNode*> visited{&root};
        while (!pending.empty())
        {
            auto node = std::move(pending.back());
            pending.pop_back();
            if (!node || !visited.insert(node.get()).second || order.size() >= 4096 ||
                node->children_.size() > 4096 || pending.size() + node->children_.size() > 4096)
            {
                if (error)
                    *error = "Invalid or oversized actor descendant tree";
                return false;
            }
            order.push_back(node);
            pending.insert(pending.end(), node->children_.rbegin(), node->children_.rend());
        }
        if (!GetRegisteredManagerForAnalysis() || (!ownedBindingMode_ && !controllers_.empty()))
        {
            if (error)
                *error = "Tree binding requires a live manager and no manual controller seam";
            return false;
        }
        ownedBindingMode_ = true;
        for (auto& node : order)
            if (!DiscoverNodeForAnalysis(node, error))
                return false;
        return true;
    }
    bool spActor::BuildBindingPlanForAnalysis(std::vector<PlaybackStateForAnalysis>& working,
                                              std::vector<spTransformTrackEval>& planned,
                                              std::optional<std::size_t> selected,
                                              std::vector<ActionForAnalysis>& actions,
                                              std::string* error)
    {
        const auto fail = [error](const char* text) {
            if (error)
                *error = text;
            return false;
        };
        auto* manager = GetRegisteredManagerForAnalysis();
        if (!manager || working.size() != states_.size() ||
            (selected && *selected >= working.size()) ||
            (!ownedBindingMode_ && !controllers_.empty()))
            return fail("Invalid actor binding context");
        for (auto& state : working)
            state.evaluation = {state.weight, state.sampleTime, &state.bindingUseCount};
        planned.resize(ownedControllers_.size());
        for (std::size_t i = 0; i < ownedControllers_.size(); ++i)
        {
            const auto& owned = ownedControllers_[i];
            const auto* node = owned.controller->GetNodeForAnalysis();
            if (!owned.name.BelongsToForAnalysis(*manager) || !node || !node->GetName() ||
                owned.name.GetNameForAnalysis() != node->GetName())
                return fail("Actor node name or manager changed after binding");
            const auto* evaluator =
                dynamic_cast<spTransformTrackEval*>(owned.controller->GetEvaluatorForAnalysis());
            if (!evaluator)
                return fail("Actor evaluator type changed");
            planned[i] = *evaluator; // host preflight snapshot, NOT native Clone
            for (auto& input : planned[i].inputs_)
                if (input.playback)
                {
                    const auto index = PlaybackIndexForAnalysis(input.playback);
                    if (!index)
                        return fail("Actor input references a foreign playback state");
                    input.playback = &working[*index].evaluation;
                    if (!working[*index].bindingUseCount)
                        input.playback = nullptr, input.sampler = nullptr;
                }
        }
        std::vector<const spAnimation*> oldAnimations(working.size());
        for (std::size_t i = 0; i < working.size(); ++i)
        {
            auto& state = working[i];
            // Native captures old pointer HERE, not once for the whole array:
            // insertion for an earlier state can change later state counters.
            oldAnimations[i] = state.bindingUseCount ? state.animation : nullptr;
            if (!state.bindingUseCount && selected != i)
                continue;
            if (!state.animation || state.animation->GetTrackCountForAnalysis() > 4096)
                return fail("Invalid actor animation track extent");
            for (std::size_t t = 0; t < state.animation->GetTrackCountForAnalysis(); ++t)
            {
                const auto* track = state.animation->GetTrackForAnalysis(t);
                if (!track->IsBoundToForAnalysis(*manager))
                    return fail("Animation tracks must hold bindings in the actor manager");
                const auto found = slotControllers_.find(track->GetBindingSlotForAnalysis());
                if (found == slotControllers_.end())
                    continue;
                if (samplers_.size() >= 4096 && !samplers_.count(track))
                    return fail("Actor prepared-sampler bound exceeded");
                auto [sampler, inserted] = samplers_.try_emplace(track);
                (void)inserted;
                sampler->second = track->GetSamplerForAnalysis();
                if (!planned[found->second].InsertInputForAnalysis(
                        {&state.evaluation, &sampler->second, state.priority, {}},
                        state.fadeMode == 0))
                    return fail("Actor binding would exceed the safe two-input capacity");
            }
        }
        for (std::size_t i = 0; i < working.size(); ++i)
        {
            const auto* current = working[i].bindingUseCount ? working[i].animation : nullptr;
            if (current == oldAnimations[i])
                continue;
            if (oldAnimations[i])
                actions.push_back({ActionKindForAnalysis::Event,
                                   i,
                                   5,
                                   {},
                                   0,
                                   0,
                                   EventPayloadForAnalysis::Playback});
            if (current)
                actions.push_back({ActionKindForAnalysis::Event,
                                   i,
                                   4,
                                   {},
                                   0,
                                   0,
                                   EventPayloadForAnalysis::Playback});
        }
        return true;
    }
    void spActor::CommitBindingPlanForAnalysis(std::vector<PlaybackStateForAnalysis>& working,
                                               std::vector<spTransformTrackEval>& planned) noexcept
    {
        for (std::size_t i = 0; i < states_.size(); ++i)
        {
            states_[i] = working[i];
            states_[i].evaluation.bindingUseCount = &states_[i].bindingUseCount;
        }
        for (std::size_t i = 0; i < planned.size(); ++i)
        {
            for (auto& input : planned[i].inputs_)
                for (std::size_t s = 0; input.playback && s < working.size(); ++s)
                    if (input.playback == &working[s].evaluation)
                    {
                        input.playback = &states_[s].evaluation;
                        break;
                    }
            *static_cast<spTransformTrackEval*>(
                ownedControllers_[i].controller->GetEvaluatorForAnalysis()) = planned[i];
        }
        ownedBindingMode_ = true;
    }
    bool spActor::RebindForAnalysis(std::optional<std::size_t> selected,
                                    std::vector<ActionForAnalysis>& actions, std::string* error)
    {
        actions.clear();
        if (error)
            error->clear();
        auto working = states_;
        std::vector<spTransformTrackEval> planned;
        std::vector<ActionForAnalysis> pending;
        if (!BuildBindingPlanForAnalysis(working, planned, selected, pending, error))
            return false;
        CommitBindingPlanForAnalysis(working, planned);
        actions = std::move(pending);
        return true;
    }
    std::optional<std::size_t> spActor::StartForAnalysis(StartRequestForAnalysis& request,
                                                         std::vector<ActionForAnalysis>& actions,
                                                         std::string* error)
    {
        actions.clear();
        if (error)
            error->clear();
        const auto fail = [error](const char* text) -> std::optional<std::size_t> {
            if (error)
                *error = text;
            return std::nullopt;
        };
        auto* manager = GetRegisteredManagerForAnalysis();
        if (!manager || !request.animation || request.mode > 3 || request.fadeMode > 4)
            return fail("Invalid actor start request/context");
        const double duration = request.animation->GetTotalTimeForAnalysis();
        if (!(duration > 0) || !std::isfinite(duration))
            return fail("Invalid actor animation duration");
        for (float value :
             {request.weight, request.fadeInDuration, request.fallbackFadeInRate,
              request.fadeOutDuration, request.fallbackFadeOutRate, request.transitionDuration,
              request.timeMultiplier, request.initialTime})
            if (!std::isfinite(value))
                return fail("Non-finite actor start request");
        std::optional<std::size_t> free, matched;
        for (std::size_t i = 0; i < states_.size(); ++i)
            if (!states_[i].bindingUseCount)
            {
                if (!free)
                    free = i;
            }
            else if (states_[i].animation == request.animation)
                matched = i;
        const auto selected = matched ? matched : free;
        if (!selected)
            return std::nullopt; // native no available slot, no request mutation
        auto working = states_;
        auto& state = working[*selected];
        float weight = request.weight;
        if (request.fadeMode == 0 || request.fadeMode == 3)
            weight = 1;
        else if (request.fadeMode == 2 || request.fadeMode == 4)
            weight = 0;
        if (!matched)
            state.weight = weight;
        state.animation = request.animation;
        state.mode = request.mode;
        state.reverse = request.reverse;
        state.fadeMode = request.fadeMode;
        state.fadeInRate = request.fadeInDuration > 0
                               ? static_cast<float>(1.0 / request.fadeInDuration)
                               : request.fallbackFadeInRate;
        state.fadeOutRate = request.fadeOutDuration > 0
                                ? static_cast<float>(1.0 / request.fadeOutDuration)
                                : request.fallbackFadeOutRate;
        state.transitionDuration = request.transitionDuration;
        state.hasLoopCallback = request.hasLoopCallback;
        state.callbackCookie = request.callbackCookie;
        state.timeMultiplier = request.timeMultiplier;
        state.running = true;
        state.priority = (request.animation->GetPriorityGroupForAnalysis() << 24) |
                         (manager->GetFrameForAnalysis() & 0xffffffU);
        state.normalizedProgress = static_cast<float>(request.initialTime / duration);
        state.fadeThreshold = static_cast<float>(duration - request.fadeOutDuration);
        state.status = request.fadeMode == 2 ? 0 : 1;
        state.elapsedTime = 0;
        // Native does not reset sampleTime, stopAfterFade, slotIndex or use count.
        for (float value : {state.weight, state.fadeInRate, state.fadeOutRate,
                            state.normalizedProgress, state.fadeThreshold})
            if (!std::isfinite(value))
                return fail("Actor start result is non-finite");
        std::vector<ActionForAnalysis> pending{{ActionKindForAnalysis::Event, *selected, 2}};
        if (request.fadeMode == 2)
            pending.push_back({ActionKindForAnalysis::Event, *selected, 6});
        std::vector<spTransformTrackEval> planned;
        if (!BuildBindingPlanForAnalysis(working, planned, selected, pending, error))
            return std::nullopt;
        pending.push_back({ActionKindForAnalysis::Flush});
        CommitBindingPlanForAnalysis(working, planned);
        request.weight = weight;
        actions = std::move(pending);
        return selected;
    }
    bool spActor::StopForAnalysis(const spAnimation* animation, bool suppressEvent,
                                  std::vector<ActionForAnalysis>& actions, std::string* error)
    {
        actions.clear();
        if (error)
            error->clear();
        if (!animation)
        {
            if (error)
                *error = "Null animation Stop is not supported by the host";
            return false;
        }
        std::size_t index = 0;
        for (; index < states_.size() && states_[index].animation != animation; ++index)
        {
        }
        if (index == states_.size())
            return true;
        auto working = states_;
        working[index].running = false;
        working[index].bindingUseCount = 0;
        std::vector<spTransformTrackEval> planned;
        std::vector<ActionForAnalysis> pending;
        if (!BuildBindingPlanForAnalysis(working, planned, {}, pending, error))
            return false;
        working[index].status = 3;
        pending.push_back({ActionKindForAnalysis::Flush});
        if (!suppressEvent)
            pending.push_back({ActionKindForAnalysis::ImmediateEvent, index, 3});
        CommitBindingPlanForAnalysis(working, planned);
        actions = std::move(pending);
        return true;
    }
    bool spActor::StopAllForAnalysis(std::vector<ActionForAnalysis>& actions, std::string* error)
    {
        actions.clear();
        if (error)
            error->clear();
        for (std::size_t i = 0; i < states_.size(); ++i)
            if (states_[i].bindingUseCount)
            {
                std::vector<ActionForAnalysis> stopped;
                if (!StopForAnalysis(states_[i].animation, false, stopped, error))
                    return false;
                actions.insert(actions.end(), stopped.begin(), stopped.end());
            }
        return true;
    }
    const spRTTIRecord& spActor::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spController::ClassID,
            "spActor",
            &spController::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spActor>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spActor::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spActor::vfunc_10(spCloneManager& manager) const
    {
        // 0x005A3680 calls the factory and inherited controller-copy slot:
        // only enabled is copied; actor states, speed and node bindings are fresh.
        auto result = std::make_unique<spActor>();
        manager.RegisterClone(*this, *result);
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    spActor::PlaybackStateForAnalysis* spActor::GetPlaybackForAnalysis(std::size_t index) noexcept
    {
        return index < states_.size() ? &states_[index] : nullptr;
    }
    bool spActor::SetControllerBindingsForAnalysis(
        std::vector<ControllerBindingForAnalysis> bindings)
    {
        if (bindings.size() > 4096 || ownedBindingMode_)
            return false;
        for (const auto& binding : bindings)
            if (binding.firstPlayback && *binding.firstPlayback >= states_.size())
                return false;
        controllers_ = std::move(bindings);
        return true;
    }
    void spActor::ApplyForAnalysis(float deltaTime)
    {
        std::vector<ActionForAnalysis> actions;
        std::string error;
        if (!TickForAnalysis(deltaTime, actions, &error))
            throw std::invalid_argument(error);
    }
    bool spActor::TickForAnalysis(float deltaTime, std::vector<ActionForAnalysis>& actions,
                                  std::string* error)
    {
        using Kind = ActionKindForAnalysis;
        actions.clear();
        if (error)
            error->clear();
        if (!applies_ && !advances_)
            return true;
        const auto fail = [error](const char* text) {
            if (error)
                *error = text;
            return false;
        };
        const float actorDelta = static_cast<float>(static_cast<double>(deltaTime) * multiplier_);
        if (!std::isfinite(actorDelta))
            return fail("Non-finite actor time step");
        // Explicit host preflight. Native assumes valid inputs and has no rollback.
        std::size_t eventBound = 0;
        for (const auto& state : states_)
        {
            if (!state.running || !state.bindingUseCount)
                continue;
            if (!state.animation || state.mode > 3 || state.fadeMode > 4)
                return fail("Unsupported active actor state");
            const double duration = state.animation->GetTotalTimeForAnalysis();
            const double step = static_cast<double>(actorDelta) * state.timeMultiplier;
            const float progress = static_cast<float>(state.normalizedProgress + step / duration);
            if (!(duration > 0) || !std::isfinite(duration) || !std::isfinite(progress))
                return fail("Invalid animation duration/progress");
            for (float value : {state.weight, state.normalizedProgress, state.timeMultiplier,
                                state.sampleTime, state.fadeInRate, state.fadeOutRate,
                                state.fadeThreshold, state.transitionDuration, state.elapsedTime,
                                static_cast<float>(state.elapsedTime + step),
                                static_cast<float>(state.weight + state.fadeInRate * step),
                                static_cast<float>(state.weight - state.fadeOutRate * step)})
                if (!std::isfinite(value))
                    return fail("Non-finite actor playback state");
            const double crossings =
                std::abs(std::floor(progress) - std::floor(state.normalizedProgress));
            if (crossings > 1024 || state.animation->GetTagsForAnalysis().size() > 4096)
                return fail("Actor traversal bound exceeded");
            eventBound += static_cast<std::size_t>(crossings + 3) *
                          (state.animation->GetTagsForAnalysis().size() + 4);
            if (eventBound > 100000)
                return fail("Actor event bound exceeded");
        }
        bool refresh = false;
        for (std::size_t index = 0; index < states_.size(); ++index)
        {
            auto& state = states_[index];
            if (!state.running || !state.bindingUseCount)
                continue;
            const double duration = state.animation->GetTotalTimeForAnalysis();
            const double step = static_cast<double>(actorDelta) * state.timeMultiplier;
            const float previousSample = state.sampleTime;
            const float progress = static_cast<float>(state.normalizedProgress + step / duration);
            std::uint32_t crossings = static_cast<std::uint32_t>(
                std::abs(std::floor(progress) - std::floor(state.normalizedProgress)));
            state.elapsedTime = static_cast<float>(state.elapsedTime + step);
            if (state.mode != 3)
                state.normalizedProgress = progress;
            if (state.elapsedTime > state.transitionDuration)
                state.transitionDuration = 0;
            bool direction = state.reverse;
            float normalizedSample = state.normalizedProgress;
            if (state.mode == 0 || state.mode == 3)
                crossings = 0;
            else if (state.mode == 1)
                normalizedSample = std::fmod(state.normalizedProgress, 1.f);
            else
            {
                const float remainder = std::fmod(state.normalizedProgress, 2.f);
                normalizedSample = remainder < 1 ? remainder : 2 - remainder;
                if (remainder >= 1)
                    direction = !direction;
            }
            state.sampleTime =
                static_cast<float>(state.reverse ? duration - duration * normalizedSample
                                                 : duration * normalizedSample);
            const auto emit = [&](std::uint32_t code, std::optional<std::uint32_t> ordinal = {}) {
                actions.push_back(
                    {Kind::Event, index, code, ordinal, 0, 0,
                     ordinal ? EventPayloadForAnalysis::Tag : EventPayloadForAnalysis::Animation});
            };
            const auto tags = [&](bool reverseOrder, const auto& predicate) {
                const auto& all = state.animation->GetTagsForAnalysis();
                if (reverseOrder)
                {
                    for (auto it = all.rbegin(); it != all.rend(); ++it)
                        if (predicate(it->time))
                            emit(11, it->wireOrdinal);
                }
                else
                    for (const auto& tag : all)
                        if (predicate(tag.time))
                            emit(11, tag.wireOrdinal);
            };
            for (std::uint32_t crossing = 0; crossing < crossings; ++crossing)
            {
                bool backwards = state.reverse;
                if (state.mode == 2)
                {
                    backwards = (crossings & 1) ? !direction : direction;
                    if (crossing & 1)
                        backwards = !backwards;
                }
                tags(backwards, [&](float time) {
                    return crossing != 0 ||
                           (backwards ? time < previousSample : time > previousSample);
                });
                if (state.hasLoopCallback)
                    actions.push_back({Kind::LoopCallback, index});
                emit(10);
                if (crossing == crossings - 1)
                {
                    const bool endBackwards = state.mode == 2 ? !backwards : backwards;
                    tags(endBackwards, [&](float time) {
                        return endBackwards ? time >= state.sampleTime : time <= state.sampleTime;
                    });
                }
            }
            if (!crossings)
                tags(direction, [&](float time) {
                    return direction ? time >= state.sampleTime && time < previousSample
                                     : time > previousSample && time <= state.sampleTime;
                });
            if (state.mode == 0 || state.mode == 3)
            {
                // Native endpoint clamp is independent of reverse flag.
                if (state.normalizedProgress > 1)
                    state.sampleTime = static_cast<float>(duration);
                else if (state.normalizedProgress < 0)
                    state.sampleTime = 0;
                if (state.normalizedProgress < 0 || state.normalizedProgress > 1 || state.mode == 3)
                {
                    state.bindingUseCount = 0;
                    state.running = false;
                    state.status = 3;
                    refresh = true;
                    emit(3);
                }
            }
            if (state.fadeMode == 2 || state.fadeMode == 4)
            {
                const double weight = state.weight + static_cast<double>(state.fadeInRate) *
                                                         state.timeMultiplier * actorDelta;
                state.weight = static_cast<float>(weight);
                if (state.fadeMode == 2)
                {
                    if (weight >= 1)
                    {
                        state.weight = 1;
                        state.fadeMode = 0;
                        emit(7);
                        state.status = 1;
                    }
                }
                else if (weight >= 1 || state.sampleTime > state.fadeThreshold)
                {
                    if (weight > 1)
                        state.weight = 1;
                    state.fadeMode = 3;
                    emit(7);
                    state.status = 2;
                }
            }
            else if (state.fadeMode == 3)
            {
                if (state.sampleTime > state.fadeThreshold)
                {
                    state.weight =
                        static_cast<float>(state.weight - static_cast<double>(state.fadeOutRate) *
                                                              state.timeMultiplier * actorDelta);
                    if (state.status != 2)
                    {
                        emit(8);
                        state.status = 2;
                    }
                }
                if (state.weight < 0)
                {
                    state.weight = 0;
                    state.fadeMode = 0;
                    emit(9);
                    if (state.stopAfterFade)
                    {
                        if (ownedBindingMode_)
                        {
                            std::vector<ActionForAnalysis> stopped;
                            if (!StopForAnalysis(state.animation, true, stopped, error))
                                return false;
                            actions.insert(actions.end(), stopped.begin(), stopped.end());
                        }
                        else
                        {
                            // Existing explicit seam mode records native Stop's
                            // rebind/flush rather than inventing controller ownership.
                            state.bindingUseCount = 0;
                            state.running = false;
                            actions.push_back({Kind::Rebind, index});
                            state.status = 3;
                            actions.push_back({Kind::Flush, index});
                        }
                    }
                }
            }
        }
        for (auto& state : states_)
        {
            state.evaluation.time = state.sampleTime;
            state.evaluation.weight = state.weight;
        }
        if (applies_)
        {
            for (auto& controller : controllers_)
            {
                actions.push_back({Kind::ControllerLookup});
                const auto firstPlayback = controller.firstPlaybackQuery
                                               ? controller.firstPlaybackQuery()
                                               : controller.firstPlayback;
                if (!firstPlayback)
                    continue;
                if (*firstPlayback >= states_.size())
                    return fail("Controller returned an invalid playback index");
                auto& state = states_[*firstPlayback];
                if (state.transitionDuration > 0)
                {
                    const float factor = state.elapsedTime / state.transitionDuration;
                    actions.push_back(
                        {Kind::Blend, *firstPlayback, 0, {}, state.sampleTime, factor});
                    if (controller.blend)
                        controller.blend(state.sampleTime, factor);
                }
                else
                {
                    actions.push_back({Kind::Direct, *firstPlayback, 0, {}, state.sampleTime});
                    if (controller.apply)
                        controller.apply(state.sampleTime);
                }
            }
            if (refresh)
            {
                if (ownedBindingMode_)
                {
                    std::vector<ActionForAnalysis> rebound;
                    if (!RebindForAnalysis({}, rebound, error))
                        return false;
                    actions.insert(actions.end(), rebound.begin(), rebound.end());
                }
                else
                    actions.push_back({Kind::Rebind});
            }
        }
        actions.push_back({Kind::Flush});
        return true;
    }
} // namespace sparkplug::reconstruction
