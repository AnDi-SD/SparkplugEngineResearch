#pragma once

// Inferred original-tree placement. PC ownership and local-PRS behavior slice;
// not a complete actor scheduler or a native-memory-compatible class.
#include "spSubController.h"
#include "spTransformEval.h"
#include "spNode.h"
#include <memory>

namespace sparkplug::reconstruction
{
    class spNodeController final : public spSubController
    {
      public:
        static constexpr spClassID ClassID = 0x14A9784E;
        spNodeController(); // native default: fresh spTransformTrackEval
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        void ApplyForAnalysis(float time) override;
        void BlendForAnalysis(float time, float factor);
        void SetNodeForAnalysis(std::shared_ptr<spNode> node) noexcept;
        // Test/dependency seam. Native constructor kind 1 allocates
        // spTransformConstEval; its defaults remain outside this slice.
        void SetEvaluatorForAnalysis(std::unique_ptr<spTransformEval> evaluator) noexcept;
        [[nodiscard]] spNode* GetNodeForAnalysis() const noexcept;
        [[nodiscard]] spTransformEval* GetEvaluatorForAnalysis() const noexcept;

      private:
        std::shared_ptr<spNode> node_;               // native intrusive owner
        std::unique_ptr<spTransformEval> evaluator_; // native direct delete
    };
} // namespace sparkplug::reconstruction
