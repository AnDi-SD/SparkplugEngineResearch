#include "wxBush.h"

#include "Analysis/PC/wxBushAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        using Prefix = wxBush::CapturedPrefixForAnalysis;

        static_assert(Abi::wxBushFirstTextOffset + Abi::wxBushTextCaptureExtent == Abi::wxBushSecondTextOffset);
        static_assert(Abi::wxBushSecondTextOffset + Abi::wxBushTextCaptureExtent <= Prefix{}.size());

        bool HasPcPrimary(const Prefix& capture) noexcept
        {
            const std::uint32_t primary = std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
            return primary == Abi::wxBushVTable;
        }
    }

    bool wxBush::TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
        CapturedPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || !HasPcPrimary(source) || !HasPcPrimary(destination))
            return false;

        const auto first = source.begin() + Abi::wxBushFirstTextOffset;
        const auto second = source.begin() + Abi::wxBushSecondTextOffset;
        const auto secondEnd = second + Abi::wxBushTextCaptureExtent;
        const auto firstZero = std::find(first, second, std::uint8_t{0});
        const auto secondZero = std::find(second, secondEnd, std::uint8_t{0});
        if (firstZero == second || secondZero == secondEnd)
            return false;

        std::copy(first, firstZero + 1, destination.begin() + Abi::wxBushFirstTextOffset);
        std::copy(second, secondZero + 1, destination.begin() + Abi::wxBushSecondTextOffset);
        return true;
    }
}
