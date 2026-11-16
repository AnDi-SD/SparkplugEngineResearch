#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction { class spNode; }

namespace winx::reconstruction
{
    // Analytical captured-value facade for the PC slot+2C state operation.
    // Captured prefixes are not native C++ objects or owned graph bindings.
    class wxMovingPlatform final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x169>;
        using CapturedPointForAnalysis = std::array<std::uint8_t, 0x24>;
        enum class NearProfileForAnalysis { Unqualified, PcPristineGlobalZeroFiniteCoordinates };
        enum class NearResultForAnalysis
        {
            Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
            GlobalWordNotAdmitted, BorrowedNodeMissing, GeometryNotAdmitted
        };

        // Qualified input has global765AB8=0 as in the pristine mapped image,
        // stable borrowed Node world cache and finite positions abs<=16384.
        // Receiver DWORD80 remains opaque in this profile. Output and returned
        // remain unchanged on refusal. Input/output prefixes may alias.
        static NearResultForAnalysis UpdateNearStateForAnalysis(
            const CapturedPrefixForAnalysis& before, std::uint32_t capturedGlobalWord,
            const sparkplug::reconstruction::spNode* borrowedNode,
            const CapturedPointForAnalysis& point, CapturedPrefixForAnalysis& output,
            bool& returned, NearProfileForAnalysis profile = NearProfileForAnalysis::Unqualified) noexcept;
    };
}
