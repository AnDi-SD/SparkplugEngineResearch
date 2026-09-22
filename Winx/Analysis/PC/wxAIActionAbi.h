#pragma once

#include "Analysis/PC/SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    using Address32 = std::uint32_t;

    // Exact allocation layout. Names are limited to construction-proven
    // fields; opaque bytes are deliberately not promoted to invented members.
    struct wxAIActionLayout final
    {
        sparkplug::evidence::pc::spBaseObjectLayout base; // 0x000
        Address32 currentAction;                           // 0x010
        std::uint8_t ownedActionContainer[0x0C];           // 0x014
        Address32 owner;                                   // 0x020
        Address32 field24;                                 // 0x024
        std::uint32_t field28;                             // 0x028: not initialized
        std::uint8_t pathFinder[0x28];                     // 0x02c
        std::uint32_t pathPoints[64][3];                   // 0x054: not initialized
        std::uint32_t pathIndex;                           // 0x354
        std::uint32_t pathPointCount;                      // 0x358
        std::uint32_t replanDeadline;                      // 0x35c
        std::uint32_t field360;                            // 0x360
        std::uint32_t pathTarget[3];                       // 0x364
        std::uint32_t durationMilliseconds;                // 0x370: 2000
        std::uint32_t clearableWord;                       // 0x374
        std::uint32_t vectorsAndWord[10];                  // 0x378
        std::uint32_t tail3A0;                             // 0x3a0: not initialized
        std::uint32_t tail3A4;                             // 0x3a4: not initialized
    };

    static_assert(sizeof(wxAIActionLayout) == 0x3A8);
    static_assert(offsetof(wxAIActionLayout, pathFinder) == 0x2C);
    static_assert(offsetof(wxAIActionLayout, pathPoints) == 0x54);
    static_assert(offsetof(wxAIActionLayout, pathIndex) == 0x354);
    static_assert(offsetof(wxAIActionLayout, clearableWord) == 0x374);

    inline constexpr std::uint32_t wxAIActionClassID = 0x490A6EB5;
    inline constexpr Address32 wxAIActionFactory = 0x00590050;
    inline constexpr Address32 wxAIActionConstructor = 0x0058FF00;
    inline constexpr Address32 wxAIActionDestructor = 0x0058FE50;
    inline constexpr Address32 wxAIActionDeletingDestructor = 0x00590030;
    inline constexpr Address32 wxAIActionClone = 0x005900B0;
    inline constexpr Address32 wxAIActionCopy = 0x0058FDD0;
    inline constexpr Address32 wxAIActionRTTIGetter = 0x0058FEF0;
    inline constexpr Address32 wxAIActionNotification = 0x0058EA60;
    inline constexpr Address32 wxAIActionClear = 0x0058EA50;
    inline constexpr Address32 wxAIActionVTable = 0x00702450;
    inline constexpr Address32 wxAIActionComplexSlot44 = 0x0058F030;
    inline constexpr Address32 wxAIActionComplexSlot48 = 0x0058F240;
}
