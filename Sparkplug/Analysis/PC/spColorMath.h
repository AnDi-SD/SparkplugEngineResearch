#pragma once
// Analytical helper, not an invented original class/API. PC424700 and4A9200
// use the exact float32 coefficient at6DCA9C for each byte conversion.
#include <array>
#include <cstdint>
namespace sparkplug::reconstruction
{
    [[nodiscard]] inline std::array<float,4> PCARGBToRGBAForAnalysis(std::uint32_t argb) noexcept
    {
        constexpr float unit=0.003921568859368563F;
        return {float((argb>>16)&255)*unit,float((argb>>8)&255)*unit,float(argb&255)*unit,float(argb>>24)*unit};
    }
}
