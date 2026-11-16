#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    // Analytical captured-prefix facade. The original full object extent,
    // physical C++ base and native construction remain unqualified.
    class wxPlayerTraits final
    {
    public:
        static constexpr std::uint32_t ClassID = 0x28C56217;
        static constexpr std::size_t CapturedPrefixBytesForAnalysis = 0x24D;
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, CapturedPrefixBytesForAnalysis>;

        enum class ResetProfileForAnalysis
        {
            Unqualified,
            PcCapturedPrimary3CReset
        };

        enum class ResetResultForAnalysis
        {
            Completed,
            UnqualifiedProfile,
            CapturedPrimaryNotAdmitted
        };

        // Refusal preserves output. Success stages the complete supplied
        // prefix and permits before/output alias. Opaque bytes are retained.
        static ResetResultForAnalysis ResetCapturedPrefixForAnalysis(
            const CapturedPrefixForAnalysis& before,
            CapturedPrefixForAnalysis& output,
            ResetProfileForAnalysis profile = ResetProfileForAnalysis::Unqualified) noexcept;
    };
}
