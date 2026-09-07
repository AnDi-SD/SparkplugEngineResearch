#pragma once

// Only the PC PRS-output contract is reconstructed. The native +0x1C
// operation, full base lifecycle and PS2 implementation are not claimed.
#include "spEvaluator.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spTransformEval : public spEvaluator
    {
      public:
        static constexpr spClassID ClassID = 0x87B0E260;
        using Vector3 = std::array<float, 3>;
        using Quaternion = std::array<float, 4>;
        struct SampleForAnalysis final
        {
            Vector3 position{};
            Quaternion rotation{};
            Vector3 scale{};
            bool hasPosition = false;
            bool hasRotation = false;
            bool hasScale = false;
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC +0x20 has seven stack arguments: time, three outputs and three
        // validity pointers. This host return value deliberately avoids
        // exposing uninitialized native outputs for invalid channels.
        [[nodiscard]] virtual SampleForAnalysis EvaluateForAnalysis(float time) = 0;
    };
} // namespace sparkplug::reconstruction
