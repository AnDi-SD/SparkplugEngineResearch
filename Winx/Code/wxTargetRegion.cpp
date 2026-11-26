#include "wxTargetRegion.h"

#include "Analysis/PC/wxTargetRegionAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        struct Field
        {
            std::size_t offset;
            std::size_t width;
        };

        constexpr Field CopiedFields[]{
            {0x144, 1}, {0x147, 1}, {0x14C, 4}, {0x148, 1},
            {0x145, 1}, {0x146, 1}, {0x151, 1},
        };
        static_assert(Abi::wxTargetRegionTextOffset + Abi::wxTargetRegionTextCaptureExtent == CopiedFields[0].offset);
        static_assert(CopiedFields[6].offset + CopiedFields[6].width <= wxTargetRegion::CapturedPrefixForAnalysis{}.size());

        std::uint32_t ReadPrimary(const wxTargetRegion::CapturedPrefixForAnalysis& capture) noexcept
        {
            return std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
        }
    }

    bool wxTargetRegion::TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
        CapturedPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || ReadPrimary(source) != Abi::wxTargetRegionVTable
            || ReadPrimary(destination) != Abi::wxTargetRegionVTable)
            return false;

        const auto text = source.begin() + Abi::wxTargetRegionTextOffset;
        const auto textEnd = text + Abi::wxTargetRegionTextCaptureExtent;
        const auto terminator = std::find(text, textEnd, std::uint8_t{0});
        if (terminator == textEnd)
            return false;

        std::copy(text, terminator + 1, destination.begin() + Abi::wxTargetRegionTextOffset);
        for (const auto& field : CopiedFields)
            std::copy_n(source.begin() + field.offset, field.width, destination.begin() + field.offset);
        return true;
    }
}
