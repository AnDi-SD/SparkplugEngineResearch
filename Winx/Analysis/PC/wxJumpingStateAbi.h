#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxJumpingStateClassID = 0x756B123A;
    inline constexpr std::uint32_t wxJumpingStateVTable = 0x006F82B0;
    inline constexpr std::uint32_t wxJumpingStateTargetMode = 0x0051AD20;
    inline constexpr std::size_t wxJumpingStateTargetModeSlotOffset = 0x38;
}
