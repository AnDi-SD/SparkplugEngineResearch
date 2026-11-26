#include "wxSpinningStatuesPuzzle.h"

#include "Analysis/PC/wxSpinningStatuesPuzzleAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        using Prefix = wxSpinningStatuesPuzzle::CapturedPrefixForAnalysis;

        struct Field
        {
            std::size_t offset;
            std::size_t width;
        };

        constexpr Field CopiedFields[]{
            {0x124, 4}, {0x138, 4}, {0x128, 4}, {0x12C, 4},
            {0x130, 4}, {0x134, 4}, {0x13C, 4}, {0x140, 4},
            {0x14C, 4}, {0x150, 4}, {0x154, 1}, {0x155, 1},
        };
        static_assert(Prefix{}.size() == Abi::wxSpinningStatuesPuzzleCaptureBytes);
        static_assert(CopiedFields[11].offset + CopiedFields[11].width <= Prefix{}.size());

        bool HasPcPrimary(const Prefix& capture) noexcept
        {
            const std::uint32_t primary = std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
            return primary == Abi::wxSpinningStatuesPuzzleVTable;
        }
    }

    bool wxSpinningStatuesPuzzle::TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
        CapturedPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || !HasPcPrimary(source) || !HasPcPrimary(destination))
            return false;

        for (const auto& field : CopiedFields)
            std::copy_n(source.begin() + field.offset, field.width, destination.begin() + field.offset);
        return true;
    }
}
