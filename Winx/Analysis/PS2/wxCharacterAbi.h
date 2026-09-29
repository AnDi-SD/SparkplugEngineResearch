#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxCharacterLayout final
    {
        std::uint8_t entityPrefix[0x130];
        std::uint8_t characterFields[0x40];
    };
    static_assert(sizeof(wxCharacterLayout) == 0x170);
    static_assert(offsetof(wxCharacterLayout, characterFields) == 0x130);

    inline constexpr std::uint32_t wxCharacterClassID = 0x0003CC73;
    inline constexpr std::uint32_t wxCharacterFactory = 0x003F77A0;
    inline constexpr std::uint32_t wxCharacterConstructor = 0x002A9C10;
    inline constexpr std::uint32_t wxCharacterVTable = 0x0049B540;
    inline constexpr std::uint32_t wxCharacterCopy = 0x002A95A0;
    inline constexpr std::uint32_t wxCharacterAssignReference = 0x002A96B0;
    inline constexpr std::uint32_t wxCharacterFlagPredicate = 0x002A9670;
    inline constexpr std::uint32_t wxCharacterConstantTrue = 0x003F98B0;
    inline constexpr std::uint32_t wxCharacterNotification = 0x002A9860;
}
