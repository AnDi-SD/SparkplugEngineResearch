#pragma once

#include <cstddef>
#include <cstdint>

namespace wxBookStateMachineAbi
{
    inline constexpr std::uint32_t ClassId = 0x396C4B37;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FAF20;
    inline constexpr std::uint32_t OwnPublisherPC = 0x0052E0F0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x228;
    inline constexpr std::size_t ScaleOffset = 0x224;
    inline constexpr std::uint32_t ScaleGetterPC = 0x0051D460;
    inline constexpr std::uint32_t ScaleSetterPC = 0x0052DE60;
}
