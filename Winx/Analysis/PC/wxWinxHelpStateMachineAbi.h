#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxWinxHelpStateMachineClassID = 0x5F303566;
    inline constexpr std::uint32_t wxWinxHelpStateMachineVTable = 0x006FB430;
    inline constexpr std::uint32_t wxWinxHelpStateMachineMaskedCode = 0x005333A0;
    inline constexpr std::size_t wxWinxHelpStateMachineMaskedCodeSlotOffset = 0x4C;
    inline constexpr std::size_t wxWinxHelpStateMachineRawCodeFieldOffset = 0x144;
}
