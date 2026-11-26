#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxChestTriggerClassID = 0x6FAF5309;
    inline constexpr std::uint32_t wxChestTriggerVTable = 0x006FBC70;
    inline constexpr std::uint32_t wxChestTriggerOwnedWordZero = 0x00537B80;
    inline constexpr std::size_t wxChestTriggerOwnedWordZeroSlotOffset = 0x48;
    inline constexpr std::size_t wxChestTriggerOwnedWordOffset = 0x398;
}
