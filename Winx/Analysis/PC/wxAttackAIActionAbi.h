#pragma once

#include "Analysis/PC/wxAIActionAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxAttackAIActionLayout final
    {
        wxAIActionLayout base;              // 0x000
        Address32 target;                   // 0x3a8
        std::uint32_t state;                // 0x3ac
        std::uint32_t deadline;             // 0x3b0
        std::uint32_t field3B4;             // 0x3b4
        std::uint32_t field3B8;             // 0x3b8
        std::uint8_t unknown3BC[0x0C];    // constructor does not initialize
        std::uint8_t pathFinder[0x28];      // 0x3c8, original inline constructor
        std::uint8_t flag3F0;              // 0x3f0
        std::uint8_t unknown3F1[3];        // constructor does not initialize
    };

    static_assert(sizeof(wxAttackAIActionLayout) == 0x3F4);
    static_assert(offsetof(wxAttackAIActionLayout, target) == 0x3A8);
    static_assert(offsetof(wxAttackAIActionLayout, pathFinder) == 0x3C8);
    inline constexpr std::uint32_t wxAttackAIActionClassID = 0x6515353D;
    inline constexpr Address32 wxAttackAIActionFactory = 0x005AA720;
    inline constexpr Address32 wxAttackAIActionConstructor = 0x005AA310;
    inline constexpr Address32 wxAttackAIActionDeletingDestructor = 0x005AA390;
    inline constexpr Address32 wxAttackAIActionClone = 0x005AA780;
    inline constexpr Address32 wxAttackAIActionCopy = 0x005AA4D0;
    inline constexpr Address32 wxAttackAIActionRTTIGetter = 0x005AA1C0;
    inline constexpr Address32 wxAttackAIActionVTable = 0x00708E50;
}
