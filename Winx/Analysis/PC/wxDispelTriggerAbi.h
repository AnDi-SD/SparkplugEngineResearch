#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxDispelTriggerClassID = 0x446244FE;
    inline constexpr std::uint32_t wxDispelTriggerVTable = 0x006FD630;
    inline constexpr std::uint32_t wxDispelTriggerSetActive = 0x00549AF0;
    inline constexpr std::uint32_t wxDispelTriggerClearActive = 0x00549B00;
    inline constexpr std::size_t wxDispelTriggerSetActiveSlotOffset = 0x54;
    inline constexpr std::size_t wxDispelTriggerClearActiveSlotOffset = 0x58;
    inline constexpr std::size_t wxDispelTriggerActiveByteOffset = 0x1B0;
}
