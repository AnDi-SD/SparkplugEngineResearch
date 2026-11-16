#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::swamp_platform_ring
{
    inline constexpr std::uint32_t PrimaryTable = 0x00700CD4;
    inline constexpr std::uint32_t RegistrationRecord = 0x00754968;
    inline constexpr std::size_t CapturedPrefixBytes = 0x134;
    inline constexpr std::array<std::size_t, 4> ScalarOffsets{0x124, 0x128, 0x12C, 0x130};
    inline constexpr std::array<std::uint32_t, 4> SetterAddresses{0x555040, 0x555060, 0x5550A0, 0x555080};
    inline constexpr std::array<std::uint32_t, 4> GetterAddresses{0x556E30, 0x580BD0, 0x57B8B0, 0x555090};
    // The historical factory allocated412bytes. This308-byte callback prefix
    // does not establish sizeof, physical inheritance, lifecycle or other fields.
}
