#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxTrollAttackState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x40>;
        using CapturedControllerPrefixForAnalysis = std::array<std::uint8_t, 0x188>;

        struct BorrowedControllerForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            CapturedControllerPrefixForAnalysis* prefix = nullptr;
        };

        // The typed controller is required only on the mark-consumption path.
        // An admitted capture returns the native AL.
        [[nodiscard]] static std::optional<std::uint8_t> TryEvaluatePcCompletionForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
            const BorrowedControllerForAnalysis& controller) noexcept;
    };
}
