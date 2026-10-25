#pragma once

#include <cmath>
#include <cstdint>

namespace sparkplug::host::particle_color
{
    // Opt-in host arithmetic for analysis/display. This is NOT original x87
    // equivalence: u=1/3, 7F132737 -> E24F65A3 gives A0263B5A here, versus
    // original A0263A5A. The faithful packer requires an explicit provider.
    [[nodiscard]] inline bool HostApproximateArgbForAnalysis(void*,std::uint32_t begin,
        std::uint32_t end,double normalizedAge,std::uint32_t& output) noexcept
    {
        if(!std::isfinite(normalizedAge))return false;
        const double complement=1.0-normalizedAge;
        std::uint32_t result=0;
        for(unsigned channel=0;channel<4;++channel)
        {
            const double first=double((begin>>(channel*8))&255)*complement;
            const double second=double((end>>(channel*8))&255)*normalizedAge;
            if(!std::isfinite(first) || !std::isfinite(second)
                || std::abs(first)>=4294967296.0 || std::abs(second)>=4294967296.0)return false;
            const auto value=channel==3?std::int64_t(std::trunc(first+second))
                :std::int64_t(std::trunc(first))+std::int64_t(std::trunc(second));
            result|=std::uint32_t(std::uint64_t(value)&255)<<(channel*8);
        }
        output=result;return true;
    }
}
