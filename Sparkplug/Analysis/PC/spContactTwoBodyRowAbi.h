#pragma once
#include "spContactConstraintAbi.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Observed native addresses/offsets for only the supplied two-body one-row path.
    // This declares no spConstraint physical base or carrier class layout.
    inline constexpr std::uint32_t spContactTwoBodyRowOwn20 = 0x48B610;
    inline constexpr std::size_t spContactTwoBodyRowOwn20Bytes = 1620;
    inline constexpr std::uint32_t spContactTwoBodyRowActualBase = 0x48B3C0;
    inline constexpr std::size_t spContactTwoBodyRowActualBaseBytes = 144;
    inline constexpr std::uint32_t spContactTwoBodyRowManager7C = 0x3E4CCCCD;
    inline constexpr std::uint32_t spContactTwoBodyRowManager80 = 0x3727C5AC;
    inline constexpr std::size_t spContactTwoBodyRowPoint = 0xB4;
    inline constexpr std::size_t spContactTwoBodyRowNormal = 0xC0;
    inline constexpr std::size_t spContactTwoBodyRowCC = 0xCC;
}
