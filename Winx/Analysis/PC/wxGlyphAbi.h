#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::analysis::pc::glyph_facing
{
    inline constexpr std::uint32_t PrimaryTable = 0x00701CE4;
    inline constexpr std::size_t CapturedPrefixBytes = 0x160;
    inline constexpr std::size_t FacingOffset = 0x15C;
    inline constexpr std::uint32_t NoOpSetterPC = 0x005B7A00;
    inline constexpr std::uint32_t FacingGetterPC = 0x0053A480;
    // The selected352B prefix equals an observed old allocation; current sizeof remains unknown.
}
