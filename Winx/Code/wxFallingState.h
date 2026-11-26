#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    // Scalar facade for the receiver-independent PC slot +0x38.
    class wxFallingState
    {
    public:
        [[nodiscard]] static std::uint32_t EvaluateTargetModeForAnalysis(
            std::uint32_t rawTarget) noexcept;
    };
}
