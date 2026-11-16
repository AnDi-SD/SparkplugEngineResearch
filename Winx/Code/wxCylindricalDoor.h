#pragma once
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Analytical captured-value facade for two published PC raw-byte fields.
    // This buffer is not a native C++ object or a constructed door graph.
    class wxCylindricalDoor final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x177>;
        enum class PropertyForAnalysis { Locked, OpenedByProximity };
        enum class PropertyProfileForAnalysis { Unqualified, PcCapturedPrimaryRawByteProperties };
        enum class PropertyResultForAnalysis
        { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, UnknownProperty };

        static PropertyResultForAnalysis ReadPropertyRawForAnalysis(
            const CapturedPrefixForAnalysis& before, PropertyForAnalysis property,
            std::uint8_t& output,
            PropertyProfileForAnalysis profile = PropertyProfileForAnalysis::Unqualified) noexcept;
        // A raw DWORD stack argument supplies its low byte. No bool
        // normalization is inferred. Input/output may alias; refusal preserves
        // output and successful writes preserve the other field and opaque bytes.
        static PropertyResultForAnalysis WritePropertyRawForAnalysis(
            const CapturedPrefixForAnalysis& before, PropertyForAnalysis property,
            std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
            PropertyProfileForAnalysis profile = PropertyProfileForAnalysis::Unqualified) noexcept;
    };
}
