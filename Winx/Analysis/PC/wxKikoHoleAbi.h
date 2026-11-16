#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::kiko_hole
{
    inline constexpr std::uint32_t PrimaryTable = 0x006F633C;
    inline constexpr std::uint32_t RegistrationRecord = 0x0074E5A8;
    inline constexpr std::size_t CapturedPrefixBytes = 0x144;
    inline constexpr std::array<std::size_t, 1> ScalarOffsets{0x140};
    inline constexpr std::array<std::uint32_t, 1> SetterAddresses{0x56A600};
    inline constexpr std::array<std::uint32_t, 1> GetterAddresses{0x4F2B00};
    // The historical factory allocated332bytes. This324-byte callback prefix
    // does not establish sizeof, physical inheritance, lifecycle or other fields.
}
