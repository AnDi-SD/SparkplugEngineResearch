#include "spTransformTrackEval.h"
#include "../../Analysis/PC/spAnimationMath.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spTransformTrackEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,
                                         spTransformEval::ClassID,
                                         "spTransformTrackEval",
                                         &spTransformEval::StaticRTTI(),
                                         +[]() -> std::unique_ptr<spBaseObject> {
                                             return std::make_unique<spTransformTrackEval>();
                                         },
                                         nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spTransformTrackEval::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spTransformTrackEval::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spTransformTrackEval>();
        manager.RegisterClone(*this, *result);
        // Native copy slot 0x0040ECE0 does not copy binding/input payload.
        return result;
    }
    bool spTransformTrackEval::SetInputsForAnalysis(std::vector<InputForAnalysis> inputs)
    {
        if (inputs.size() > 2)
            return false;
        for (const auto& input : inputs)
            if (input.sampler && (!input.playback || !*input.sampler))
                return false;
        inputCount_ = inputs.size();
        std::copy(inputs.begin(), inputs.end(), inputs_.begin());
        // Literal fixture setter, not native insertion. Clear inactive borrowed
        // pointers for host safety, retain physical priority/cache residue.
        for (std::size_t index = inputCount_; index < inputs_.size(); ++index)
        {
            inputs_[index].playback = nullptr;
            inputs_[index].sampler = nullptr;
        }
        return true;
    }
    std::vector<spTransformTrackEval::InputForAnalysis> spTransformTrackEval::GetInputsForAnalysis()
        const
    {
        return {inputs_.begin(), inputs_.begin() + inputCount_};
    }
    const std::array<spTransformTrackEval::InputForAnalysis, 2>& spTransformTrackEval::
        GetPhysicalInputsForAnalysis() const noexcept
    {
        return inputs_;
    }
    std::size_t spTransformTrackEval::GetInputCountForAnalysis() const noexcept
    {
        return inputCount_;
    }
    bool spTransformTrackEval::ClearInputForAnalysis(std::size_t index) noexcept
    {
        if (index >= inputs_.size())
            return false;
        inputs_[index].playback = nullptr;
        inputs_[index].sampler = nullptr;
        return true; // original changes neither count, priority, caches nor use counter
    }
    bool spTransformTrackEval::InsertInputForAnalysis(InputForAnalysis input, bool exclusive)
    {
        if (!input.playback || !input.playback->bindingUseCount ||
            (input.sampler && !*input.sampler))
            return false;
        // Include inactive physical slot1: exclusive insertion accesses it even
        // if count is only one. These are borrowed live-state preconditions.
        for (const auto& old : inputs_)
            if (old.playback && !old.playback->bindingUseCount)
                return false;
        if (exclusive)
        {
            for (std::size_t index = 0; index < inputCount_; ++index)
                if (inputs_[index].playback && inputs_[index].priority > input.priority)
                    return true;
            // Preserve destination key cache; original does NOT decrement the
            // old first slot, including repeated insertion of the same state.
            inputs_[0].playback = input.playback;
            inputs_[0].sampler = input.sampler;
            inputs_[0].priority = input.priority;
            ++*input.playback->bindingUseCount;
            inputCount_ = 1;
            if (inputs_[1].playback)
                --*inputs_[1].playback->bindingUseCount;
            (void)ClearInputForAnalysis(1);
            return true;
        }
        std::array<int, 3> order{};
        std::size_t count = 0;
        bool inserted = false;
        for (std::size_t index = 0; index < inputCount_; ++index)
        {
            const auto& old = inputs_[index];
            if (!old.playback)
                continue;
            if (!inserted && old.priority > input.priority)
            {
                order[count++] = -1;
                inserted = true;
            }
            if (old.playback != input.playback)
                order[count++] = static_cast<int>(index);
        }
        if (!inserted)
            order[count++] = -1;
        if (count > 2)
            return false; // host atomic safety fence; no such native local guard
        const auto old = inputs_;
        for (std::size_t index = 0; index < inputCount_; ++index)
            if (old[index].playback == input.playback)
                --*input.playback->bindingUseCount;
        inputCount_ = count;
        for (std::size_t index = 0; index < count; ++index)
        {
            if (order[index] >= 0)
                inputs_[index] = old[static_cast<std::size_t>(order[index])];
            else
            {
                // New/reinserted state inherits cache at the physical output
                // slot. Only retained old inputs carry their cache when moved.
                inputs_[index].playback = input.playback;
                inputs_[index].sampler = input.sampler;
                inputs_[index].priority = input.priority;
                ++*input.playback->bindingUseCount;
            }
        }
        return true;
    }
    std::int32_t spTransformTrackEval::GetBoundSlotForAnalysis() const noexcept
    {
        return boundSlot_;
    }
    void spTransformTrackEval::SetBoundSlotForAnalysis(std::int32_t slot) noexcept
    {
        boundSlot_ = slot;
    }

    spTransformEval::SampleForAnalysis spTransformTrackEval::EvaluateForAnalysis(float time)
    {
        (void)time; // Native body ignores the outer time and reads input state +0x34.
        std::array<SampleForAnalysis, 2> sampled{};
        std::array<float, 2> weights{};
        std::size_t count = 0;
        for (std::size_t index = 0; index < inputCount_; ++index)
        {
            auto& input = inputs_[index];
            if (!input.sampler)
                continue;
            sampled[count] = (*input.sampler)(input.playback->time, input.cache);
            weights[count++] = input.playback->weight;
        }
        if (count == 0)
            return {};
        if (count == 1)
            return sampled[0];
        SampleForAnalysis result;
        float cumulative = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const float previous = cumulative;
            cumulative += weights[i];
            const float oldFactor = previous / cumulative;
            const auto& current = sampled[i];
            const auto combine = [oldFactor](Vector3& out, const Vector3& value, bool valid) {
                if (!valid)
                {
                    out = value;
                    return;
                }
                for (std::size_t axis = 0; axis < 3; ++axis)
                    out[axis] = oldFactor * out[axis] + (1 - oldFactor) * value[axis];
            };
            if (current.hasPosition)
                combine(result.position, current.position, result.hasPosition);
            if (current.hasScale)
                combine(result.scale, current.scale, result.hasScale);
            if (current.hasRotation)
            {
                if (!result.hasRotation)
                    result.rotation = current.rotation;
                else if (oldFactor > 0)
                    result.rotation = evidence::pc::animation_math::Interpolate(
                        result.rotation, current.rotation, 1 - oldFactor);
            }
            result.hasPosition |= current.hasPosition;
            result.hasRotation |= current.hasRotation;
            result.hasScale |= current.hasScale;
        }
        return result;
    }
} // namespace sparkplug::reconstruction
