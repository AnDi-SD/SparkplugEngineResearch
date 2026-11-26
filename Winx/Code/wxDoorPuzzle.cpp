#include "wxDoorPuzzle.h"

#include "Analysis/PC/wxDoorPuzzleAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        using Prefix = wxDoorPuzzle::CapturedPrefixForAnalysis;

        struct Field
        {
            std::size_t offset;
            std::size_t width;
        };

        constexpr Field CopiedFields[]{
            {0x130, 4}, {0x134, 4}, {0x138, 4}, {0x13C, 4}, {0x140, 4},
            {0x144, 4}, {0x15C, 4}, {0x160, 4}, {0x164, 4}, {0x168, 1},
            {0x169, 1}, {0x16C, 4}, {0x124, 4}, {0x128, 4}, {0x12C, 4},
        };
        static_assert(Prefix{}.size() == Abi::wxDoorPuzzleCaptureBytes);

        bool HasPcPrimary(const Prefix& capture) noexcept
        {
            const std::uint32_t primary = std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
            return primary == Abi::wxDoorPuzzleVTable;
        }
    }

    bool wxDoorPuzzle::TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
        CapturedPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || !HasPcPrimary(source) || !HasPcPrimary(destination))
            return false;

        for (const auto& field : CopiedFields)
            std::copy_n(source.begin() + field.offset, field.width, destination.begin() + field.offset);
        return true;
    }
}
