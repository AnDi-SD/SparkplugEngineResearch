#pragma once
#include "spContactConstraintAbi.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Observed native addresses/offsets for only the supplied one-row path.
    // This declares no spConstraint physical base or carrier class layout.
    inline constexpr std::uint32_t spContactSingleBodyRowOwn20 = 0x48B610;
    inline constexpr std::size_t spContactSingleBodyRowOwn20Bytes = 1620;
    inline constexpr std::uint32_t spContactSingleBodyRowActualBase = 0x48B3C0;
    inline constexpr std::size_t spContactSingleBodyRowActualBaseBytes = 144;
    inline constexpr std::uint32_t spContactSingleBodyRowManager7C = 0x3E4CCCCD;
    inline constexpr std::uint32_t spContactSingleBodyRowManager80 = 0x3727C5AC;
    inline constexpr std::size_t spContactSingleBodyRowPoint = 0xB4;
    inline constexpr std::size_t spContactSingleBodyRowNormal = 0xC0;
    inline constexpr std::size_t spContactSingleBodyRowCC = 0xCC;
}
