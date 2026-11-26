#include "spDXEnvironmentMapLayer.h"

#include "Analysis/PC/spDXEnvironmentMapLayerAbi.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace Abi = sparkplug::evidence::pc;
        using Prefix = spDXEnvironmentMapLayer::CapturedPcPrefixForAnalysis;

        static_assert(spDXEnvironmentMapLayer::ClassID == Abi::DXEnvironmentMapLayerClassId);
        static_assert(Prefix{}.size() == Abi::DXEnvironmentMapLayerCaptureBytes);
        static_assert(Abi::DXEnvironmentMapLayerOwnWordOffset + Abi::DXEnvironmentMapLayerOwnWordBytes == Prefix{}.size());

        bool HasPcPrimary(const Prefix& capture) noexcept
        {
            const std::uint32_t primary = std::uint32_t(capture[0]) | (std::uint32_t(capture[1]) << 8)
                | (std::uint32_t(capture[2]) << 16) | (std::uint32_t(capture[3]) << 24);
            return primary == Abi::DXEnvironmentMapLayerPrimary;
        }
    }

    bool spDXEnvironmentMapLayer::TryCopyPcPayloadForAnalysis(const CapturedPcPrefixForAnalysis& source,
        CapturedPcPrefixForAnalysis& destination) noexcept
    {
        if (&source == &destination || !HasPcPrimary(source) || !HasPcPrimary(destination))
            return false;

        constexpr auto offset = Abi::DXEnvironmentMapLayerOwnWordOffset;
        std::copy_n(source.data() + offset, Abi::DXEnvironmentMapLayerOwnWordBytes, destination.data() + offset);
        return true;
    }
}
