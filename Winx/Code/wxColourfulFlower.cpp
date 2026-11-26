#include "wxColourfulFlower.h"

#include "Analysis/PC/wxColourfulFlowerAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        using Prefix = wxColourfulFlower::CapturedPrefixForAnalysis;

        static_assert(Abi::wxColourfulFlowerWordOffset + 4 == Prefix{}.size());

        bool HasPcPrimary(const Prefix& capture) noexcept
        {
            const std::uint32_t primary = std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
            return primary == Abi::wxColourfulFlowerVTable;
        }
    }

    bool wxColourfulFlower::TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
        CapturedPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || !HasPcPrimary(source) || !HasPcPrimary(destination))
            return false;

        destination[Abi::wxColourfulFlowerFirstByteOffset] = source[Abi::wxColourfulFlowerFirstByteOffset];
        destination[Abi::wxColourfulFlowerSecondByteOffset] = source[Abi::wxColourfulFlowerSecondByteOffset];
        std::copy_n(source.begin() + Abi::wxColourfulFlowerWordOffset, 4,
            destination.begin() + Abi::wxColourfulFlowerWordOffset);
        return true;
    }
}
