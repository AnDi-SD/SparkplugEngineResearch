#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::cylindrical_door
{
    inline constexpr std::uint32_t ClassId = 0x47D64803;
    inline constexpr std::uint32_t PrimaryTable = 0x00700AB0;
    inline constexpr std::uint32_t RttiRecord = 0x007548A8;
    inline constexpr std::uint32_t LockedSetter = 0x005799A0;
    inline constexpr std::uint32_t LockedGetter = 0x005799B0;
    inline constexpr std::uint32_t OpenedByProximitySetter = 0x00579980;
    inline constexpr std::uint32_t OpenedByProximityGetter = 0x00579990;
    inline constexpr std::size_t LockedOffset = 0x176;
    inline constexpr std::size_t OpenedByProximityOffset = 0x175;
    inline constexpr std::size_t CapturedPrefixBytes = 0x177;
    // Minimum touched captured prefix; the historical factory allocation
    // measured380 bytes. Neither observation recovers the full native layout.
}
