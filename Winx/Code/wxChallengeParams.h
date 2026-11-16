#pragma once
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Captured values for an interior initializer called by the PC constructor.
    // This prefix is not a native object or a recovered full class layout.
    class wxChallengeParams final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x170>;
        enum class DefaultsProfileForAnalysis { Unqualified, PcCapturedPrimaryInterior124 };
        enum class DefaultsResultForAnalysis { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted };

        // The original helper receives parent-this+0x124 and writes19DWORDs.
        // This facade returns the whole captured value, preserving all292
        // preceding opaque bytes. Input/output may alias. Refusal preserves output.
        static DefaultsResultForAnalysis ResetParameterDefaultsForAnalysis(
            const CapturedPrefixForAnalysis& before, CapturedPrefixForAnalysis& output,
            DefaultsProfileForAnalysis profile = DefaultsProfileForAnalysis::Unqualified) noexcept;
    };
}
