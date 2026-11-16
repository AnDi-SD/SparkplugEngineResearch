#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::side_quest_given_object
{
    inline constexpr std::uint32_t PrimaryTable = 0x006FDFE0;
    inline constexpr std::uint32_t RegistrationRecord = 0x00753588;
    inline constexpr std::size_t CapturedPrefixBytes = 0x134;
    inline constexpr std::array<std::size_t, 1> ScalarOffsets{0x130};
    inline constexpr std::array<std::uint32_t, 1> SetterAddresses{0x555080};
    inline constexpr std::array<std::uint32_t, 1> GetterAddresses{0x555090};
    // The historical factory allocated312bytes. This308-byte callback prefix
    // does not establish sizeof, physical inheritance, lifecycle or other fields.
}
