#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    struct wxHurtStateLayout final { wxCharacterStateLayout base; std::uint8_t byte3C, padding3D[3]; };
    static_assert(sizeof(wxHurtStateLayout) == 0x40);
    static_assert(offsetof(wxHurtStateLayout, byte3C) == 0x3c);
    inline constexpr Address32 HurtTable = 0x006f8518;
    inline constexpr Address32 HurtFactory = 0x004027e0;
    inline constexpr Address32 HurtClone = 0x004093e0;
    inline constexpr Address32 HurtEntry = 0x00515a80;
    inline constexpr Address32 HurtUpdate = 0x00523690;
    inline constexpr Address32 HurtPermission = 0x005a6770;
    inline constexpr Address32 HurtTransitionPermission = 0x005159f0;
}
