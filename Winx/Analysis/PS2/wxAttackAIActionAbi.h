#pragma once

#include "Analysis/PS2/wxAIActionAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxAttackAIActionLayout final
    {
        wxAIActionLayout base;              // 0x000
        Address32 target;                   // 0x3ac
        std::uint32_t state;                // 0x3b0
        std::uint32_t deadline;             // 0x3b4
        std::uint32_t field3B8;             // 0x3b8
        std::uint32_t field3BC;             // 0x3bc
        std::uint8_t unknown3C0[0x0C];    // constructor does not initialize
        std::uint8_t pathFinder[0x28];      // 0x3cc
        std::uint8_t flag3F4;              // 0x3f4
        std::uint8_t unknown3F5[3];        // constructor does not initialize
    };

    static_assert(sizeof(wxAttackAIActionLayout) == 0x3F8);
    static_assert(offsetof(wxAttackAIActionLayout, target) == 0x3AC);
    static_assert(offsetof(wxAttackAIActionLayout, pathFinder) == 0x3CC);
    inline constexpr std::uint32_t wxAttackAIActionClassID = 0x6515353D;
    inline constexpr Address32 wxAttackAIActionFactory = 0x00244030;
    // PS2 factory inlines the derived field stores after calling base ctor.
    inline constexpr Address32 wxAttackAIActionFactoryConstruction = 0x00244060;
    inline constexpr Address32 wxAttackAIActionDeletingDestructor = 0x00243EA0;
    inline constexpr Address32 wxAttackAIActionClone = 0x00243F70;
    inline constexpr Address32 wxAttackAIActionCopy = 0x00243B00;
    inline constexpr Address32 wxAttackAIActionRTTIGetter = 0x00242CD0;
    inline constexpr Address32 wxAttackAIActionVTable = 0x00492350;
}
