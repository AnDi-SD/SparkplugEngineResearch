#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::character_traits
{
    // PC identity and touched prefix only; full native sizeof remains unknown.
    inline constexpr std::uint32_t ClassId = 0x1BC6108E;
    inline constexpr std::uint32_t PrimaryTable = 0x006F67F0;
    inline constexpr std::uint32_t RttiRecord = 0x0074E8A8;
    inline constexpr std::uint32_t PropertySetter = 0x004FC8A0;
    inline constexpr std::uint32_t PropertyGetter = 0x004FC8B0;
    inline constexpr std::size_t RawPropertyOffset = 0x1A4;
    inline constexpr std::size_t CapturedPrefixBytes = 0x1A8;
}
