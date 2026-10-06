#include "spTransformConstEval.h"
#include "../../Analysis/PC/spTransformConstMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spTransformConstEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spTransformEval::ClassID,
            "spTransformConstEval", &spTransformEval::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> {
                return std::make_unique<spTransformConstEval>();
            }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }

    const spRTTIRecord& spTransformConstEval::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    std::unique_ptr<spBaseObject> spTransformConstEval::vfunc_10(spCloneManager& manager) const
    {
        // PC601D10 calls the inherited40ECE0 copy. Own PRS/function/flags
        // remain constructor defaults, including scale-function type zero.
        auto result = std::make_unique<spTransformConstEval>();
        manager.RegisterCloneForAnalysis(*this, *result);
        return spBaseObject::vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }

    spTransformConstEval::StateForAnalysis spTransformConstEval::GetStateForAnalysis() const noexcept
    {
        auto result = state_;
        result.scaleFunction = scaleFunction_.GetStateForAnalysis();
        return result;
    }

    void spTransformConstEval::SetStateForAnalysis(const StateForAnalysis& state) noexcept
    {
        state_ = state;
        scaleFunction_.SetStateForAnalysis(state.scaleFunction);
    }

    bool spTransformConstEval::EvaluateInPlaceForAnalysis(float time, SampleForAnalysis& sample)
    {
        const auto finite = [](const auto& values) {
            return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
        };
        if ((state_.positionEnabled && (!std::isfinite(time) || !finite(state_.velocity) || !finite(sample.position))) ||
            (state_.rotationEnabled && (!std::isfinite(state_.angle) || !finite(state_.axis) || !finite(sample.rotation))))
            return false;

        auto next = sample;
        next.hasPosition = state_.positionEnabled != 0;
        next.hasRotation = state_.rotationEnabled != 0;
        next.hasScale = scaleFunction_.GetStateForAnalysis().functionType != 0;
        if (next.hasPosition)
            for (std::size_t index = 0; index < 3; ++index)
                next.position[index] = evidence::pc::transform_const_math::AddProduct(
                    sample.position[index], time, state_.velocity[index], index != 0);
        if (next.hasRotation)
            next.rotation = evidence::pc::transform_const_math::Rotate(state_.angle, state_.axis, sample.rotation);
        if ((next.hasPosition && !finite(next.position)) || (next.hasRotation && !finite(next.rotation)))
            return false;
        if (next.hasScale)
        {
            float scalar = 0;
            if (!scaleFunction_.EvaluateForAnalysis(time, scalar))
                return false;
            next.scale.fill(scalar);
        }
        sample = next;
        return true;
    }

    spTransformEval::SampleForAnalysis spTransformConstEval::EvaluateForAnalysis(float time)
    {
        SampleForAnalysis result;
        result.rotation = {0, 0, 0, 1};
        result.scale = {1, 1, 1};
        if (!EvaluateInPlaceForAnalysis(time, result))
            throw std::runtime_error("spTransformConstEval requires supported finite inputs");
        return result;
    }
}
