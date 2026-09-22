#pragma once

#include "Analysis/PC/SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    using Address32 = std::uint32_t;
    // This is the physical PC allocation used by wxCharacterState and
    // wxActionState. Names are limited to fields touched by recovered slots.
    struct wxCharacterStateLayout final
    {
        sparkplug::evidence::pc::spBaseObjectLayout base; // 0x00
        std::uint32_t stateSelector;                      // 0x10
        Address32 owner;                                  // 0x14
        Address32 completionConsumer;                     // 0x18
        std::uint8_t transitionFlag1C;                    // 0x1c
        std::uint8_t transitionFlag1D;                    // 0x1d
        std::uint8_t transitionFlag1E;                    // 0x1e
        std::uint8_t transitionFlag1F;                    // 0x1f
        std::uint8_t transitionFlag20;                    // 0x20
        std::uint8_t padding21[3];                        // 0x21
        Address32 pendingHandle;                          // 0x24
        Address32 field28;                                // 0x28
        std::uint8_t field2C;                             // 0x2c
        std::uint8_t padding2D[3];                        // 0x2d
        std::uint32_t resetValues[3];                     // 0x30
    };

    static_assert(sizeof(wxCharacterStateLayout) == 0x3C);
    static_assert(offsetof(wxCharacterStateLayout, stateSelector) == 0x10);
    static_assert(offsetof(wxCharacterStateLayout, pendingHandle) == 0x24);
    static_assert(offsetof(wxCharacterStateLayout, resetValues) == 0x30);

    inline constexpr std::uint32_t wxCharacterStateClassID = 0x44817BC2;
    inline constexpr std::uint32_t wxActionStateClassID = 0x196333F8;
    inline constexpr Address32 wxCharacterStateFactory = 0x00401C20;
    inline constexpr Address32 wxCharacterStateVTable = 0x006F81B0;
    inline constexpr Address32 wxActionStateFactory = 0x00402F60;
    // Construction is in the protected factory; this is the non-deleting dtor.
    inline constexpr Address32 wxActionStateDestructor = 0x00518F60;
    inline constexpr Address32 wxActionStateDeletingDestructor = 0x00518F70;
    inline constexpr Address32 wxActionStateVTable = 0x006F8D30;
    inline constexpr Address32 wxActionStateSlot1C = 0x00519000;
    inline constexpr Address32 wxActionStateSlot20 = 0x005190B0;
    inline constexpr Address32 wxActionStateSlot30 = 0x00518F90;
}
