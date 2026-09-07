#include "spNodeController.h"
#include "spTransformTrackEval.h"
#include "../../Analysis/PC/spAnimationMath.h"
#include <cmath>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        template <std::size_t Size>
        bool Different(const std::array<float, Size>& a, const std::array<float, Size>& b)
        {
            for (std::size_t i = 0; i < Size; ++i)
                if (std::fabs(a[i] - b[i]) > 0.001F)
                    return true;
            return false;
        }
    } // namespace
    spNodeController::spNodeController() : evaluator_(std::make_unique<spTransformTrackEval>())
    {
    }
    const spRTTIRecord& spNodeController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spSubController::ClassID,
            "spNodeController",
            &spSubController::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spNodeController>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spNodeController::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spNodeController::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spNodeController>();
        manager.RegisterClone(*this, *result);
        return result; // Native copy slot is the base no-payload stub.
    }
    void spNodeController::SetNodeForAnalysis(std::shared_ptr<spNode> node) noexcept
    {
        node_ = std::move(node);
    }
    void spNodeController::SetEvaluatorForAnalysis(
        std::unique_ptr<spTransformEval> evaluator) noexcept
    {
        evaluator_ = std::move(evaluator);
    }
    spNode* spNodeController::GetNodeForAnalysis() const noexcept
    {
        return node_.get();
    }
    spTransformEval* spNodeController::GetEvaluatorForAnalysis() const noexcept
    {
        return evaluator_.get();
    }

    void spNodeController::ApplyForAnalysis(float time)
    {
        if (!node_ || !evaluator_)
            return; // explicit host-only safety guard
        const auto sample = evaluator_->EvaluateForAnalysis(time);
        if (sample.hasPosition)
        {
            node_->SetPositionForAnalysis(sample.position);
            node_->MarkLocalTransformDirtyForAnalysis();
        }
        if (sample.hasRotation)
        {
            node_->SetOrientationForAnalysis(
                evidence::pc::animation_math::ToMatrix(sample.rotation));
            node_->MarkLocalTransformDirtyForAnalysis();
        }
        if (sample.hasScale)
        {
            node_->SetScaleForAnalysis(sample.scale);
            node_->MarkLocalTransformDirtyForAnalysis();
        }
    }
    void spNodeController::BlendForAnalysis(float time, float factor)
    {
        if (!node_ || !evaluator_)
            return;
        const auto sample = evaluator_->EvaluateForAnalysis(time);
        const auto& oldPosition = node_->GetPositionForAnalysis();
        if (sample.hasPosition && Different(oldPosition, sample.position))
        {
            auto position = oldPosition;
            for (std::size_t i = 0; i < 3; ++i)
                position[i] = sample.position[i] * factor + oldPosition[i] * (1 - factor);
            node_->SetPositionForAnalysis(position);
            node_->MarkLocalTransformDirtyForAnalysis();
        }
        if (sample.hasRotation)
        {
            namespace math = evidence::pc::animation_math;
            const auto previous = math::FromMatrix(node_->GetOrientationForAnalysis());
            if (Different(previous, sample.rotation))
            {
                node_->SetOrientationForAnalysis(
                    math::ToMatrix(math::Interpolate(previous, sample.rotation, factor)));
                node_->MarkLocalTransformDirtyForAnalysis();
            }
        }
        if (sample.hasScale)
        {
            node_->SetScaleForAnalysis(sample.scale); // not interpolated in native PC
            node_->MarkLocalTransformDirtyForAnalysis();
        }
    }
} // namespace sparkplug::reconstruction
