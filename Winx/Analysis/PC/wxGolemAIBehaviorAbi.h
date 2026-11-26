#pragma once

#include <cstddef>
#include <cstdint>

namespace wxGolemAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x63655628;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FFA90;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00566680;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1DC;
    inline constexpr std::size_t StunnedTimeRawOffset = 0x1D8;
    inline constexpr std::uint32_t StunnedTimeSetterPC = 0x00565BF0;
    inline constexpr std::uint32_t StunnedTimeGetterPC = 0x004FD7B0;
}
