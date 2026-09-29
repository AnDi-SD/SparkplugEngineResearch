#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxCharacterLayout final
    {
        std::uint8_t entityPrefix[0x124];
        std::uint8_t characterFields[0x3C];
    };
    static_assert(sizeof(wxCharacterLayout) == 0x160);
    static_assert(offsetof(wxCharacterLayout, characterFields) == 0x124);

    inline constexpr std::uint32_t wxCharacterClassID = 0x0003CC73;
    inline constexpr std::uint32_t wxCharacterFactory = 0x004015E0;
    inline constexpr std::uint32_t wxCharacterVTable = 0x006F6438;
    inline constexpr std::uint32_t wxCharacterDestructor = 0x004F3E00;
    inline constexpr std::uint32_t wxCharacterCopy = 0x004F40A0;
    inline constexpr std::uint32_t wxCharacterAssignReference = 0x004F4040;
    inline constexpr std::uint32_t wxCharacterFlagPredicate = 0x004F4070;
    inline constexpr std::uint32_t wxCharacterConstantTrue = 0x004F3DF0;
    inline constexpr std::uint32_t wxCharacterNotification = 0x004F45A0;
}
