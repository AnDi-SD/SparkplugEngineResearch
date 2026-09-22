#pragma once

#include "Analysis/PS2/SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    using Address32 = std::uint32_t;

    struct wxAIActionLayout final
    {
        sparkplug::evidence::ps2::spBaseObjectLayout base; // 0x000
        Address32 currentAction;                            // 0x010
        std::uint8_t ownedActionContainer[0x10];            // 0x014
        Address32 owner;                                    // 0x024
        Address32 field28;                                  // 0x028
        std::uint32_t field2C;                              // 0x02c: not initialized
        std::uint8_t pathFinder[0x28];                      // 0x030
        std::uint32_t pathPoints[64][3];                    // 0x058: not initialized
        std::uint32_t pathIndex;                            // 0x358
        std::uint32_t pathPointCount;                       // 0x35c
        std::uint32_t replanDeadline;                       // 0x360
        std::uint32_t field364;                             // 0x364
        std::uint32_t pathTarget[3];                        // 0x368
        std::uint32_t durationMilliseconds;                 // 0x374: 2000
        std::uint32_t clearableWord;                        // 0x378
        std::uint32_t vectorsAndWord[10];                   // 0x37c
        std::uint32_t tail3A4;                              // 0x3a4: not initialized
        std::uint32_t tail3A8;                              // 0x3a8: not initialized
    };

    static_assert(sizeof(wxAIActionLayout) == 0x3AC);
    static_assert(offsetof(wxAIActionLayout, pathFinder) == 0x30);
    static_assert(offsetof(wxAIActionLayout, pathPoints) == 0x58);
    static_assert(offsetof(wxAIActionLayout, pathIndex) == 0x358);
    static_assert(offsetof(wxAIActionLayout, clearableWord) == 0x378);

    inline constexpr std::uint32_t wxAIActionClassID = 0x490A6EB5;
    inline constexpr Address32 wxAIActionFactory = 0x00223850;
    inline constexpr Address32 wxAIActionConstructor = 0x002236A0;
    inline constexpr Address32 wxAIActionDeletingDestructor = 0x002234F0;
    inline constexpr Address32 wxAIActionClone = 0x00223790;
    inline constexpr Address32 wxAIActionCopy = 0x00223250;
    inline constexpr Address32 wxAIActionRTTIGetter = 0x00221D40;
    inline constexpr Address32 wxAIActionNotification = 0x00223400;
    inline constexpr Address32 wxAIActionClear = 0x00223490;
    inline constexpr Address32 wxAIActionVTable = 0x00492060;
    inline constexpr Address32 wxAIActionComplexSlot44 = 0x00222F10;
    inline constexpr Address32 wxAIActionComplexSlot48 = 0x00222E40;
}
