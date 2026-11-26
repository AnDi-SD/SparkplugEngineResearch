#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxKickableIceAbi
{
    inline constexpr std::uint32_t ClassId = 0x7F284E80;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FCB38;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00543340;
    inline constexpr std::size_t CapturedPrefixBytes = 0x1FC;
    inline constexpr std::array<std::size_t, 3> RawOffsets{0x1E4, 0x1F4, 0x16C};
    inline constexpr std::array<std::uint32_t, 3> RawSettersPC{0x0050A5B0, 0x005427A0, 0x005427D0};
    inline constexpr std::array<std::uint32_t, 3> RawGettersPC{0x00542790, 0x0050BCD0, 0x00510160};

    inline constexpr std::size_t ImpulseRangeOffset = 0x170;
    inline constexpr std::uint32_t ImpulseRangeSetterPC = 0x005427C0;
    inline constexpr std::uint32_t ImpulseRangeGetterPC = 0x005427B0;
}
