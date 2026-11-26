#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxTrollMovingStateClassID = 0x16744A8E;
    inline constexpr std::uint32_t wxTrollMovingStateVTable = 0x006F91D8;
    inline constexpr std::uint32_t wxTrollMovingStateAirEvent = 0x0051A680;
    inline constexpr std::size_t wxTrollMovingStateAirEventSlotOffset = 0x3C;
    inline constexpr std::size_t wxTrollMovingStateAirEventByteOffset = 0x3D;

    inline constexpr std::uint32_t wxYetiMovingStateClassID = 0x2A7B1428;
    inline constexpr std::uint32_t wxYetiMovingStateVTable = 0x006F90C8;
    inline constexpr std::uint32_t wxYetiMovingStateAirEvent = 0x0051A0C0;
    inline constexpr std::size_t wxYetiMovingStateAirEventSlotOffset = 0x3C;
    inline constexpr std::size_t wxYetiMovingStateAirEventByteOffset = 0x3C;
}
