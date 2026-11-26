#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxShadowBeastAttackState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x58>;
        using CapturedControllerPrefixForAnalysis = std::array<std::uint8_t, 0x188>;

        struct BorrowedControllerForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            CapturedControllerPrefixForAnalysis* prefix = nullptr;
        };

        // Addresses identify detached PC spans. Only the delegated path binds
        // the controller. The result contains the observed AL on admission.
        [[nodiscard]] static std::optional<std::uint8_t> TryEvaluatePcPermissionForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
            std::uint32_t capturedStateAddress, const BorrowedControllerForAnalysis& controller) noexcept;
    };
}
