#pragma once

#include <cstddef>
#include <cstdint>

namespace wxWandringNPCAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x5FC363D2;
    inline constexpr std::uint32_t CapturedPrimary = 0x00700528;
    inline constexpr std::uint32_t OwnPublisherPC = 0x005713D0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1D0;
    inline constexpr std::size_t AmplitudeOffset = 0x1CC;
    inline constexpr std::uint32_t AmplitudeGetterPC = 0x00570CD0;
    inline constexpr std::uint32_t AmplitudeSetterPC = 0x00562310;
}
