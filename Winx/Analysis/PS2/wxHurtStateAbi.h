#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    struct wxHurtStateLayout final { wxCharacterStateLayout base; std::uint8_t byte3C, padding3D[3]; };
    static_assert(sizeof(wxHurtStateLayout) == 0x40);
    static_assert(offsetof(wxHurtStateLayout, byte3C) == 0x3c);
    inline constexpr Address32 HurtTable = 0x0049a6c0;
    inline constexpr Address32 HurtFactory = 0x003f47a0;
    inline constexpr Address32 HurtClone = 0x003f46e0;
    inline constexpr Address32 HurtConstructor = 0x002ce820;
    inline constexpr Address32 HurtEntry = 0x002ce3c0;
    inline constexpr Address32 HurtUpdate = 0x002ce3b0;
    inline constexpr Address32 HurtPermission = 0x002ce790;
    inline constexpr Address32 HurtTransitionPermission = 0x002ce6e0;
}
