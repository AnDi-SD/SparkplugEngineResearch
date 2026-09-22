#pragma once

#include "Analysis/PS2/SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    using Address32 = std::uint32_t;
    // The paired PC/PS2 factories allocate the same 0x3c-byte state layout.
    struct wxCharacterStateLayout final
    {
        sparkplug::evidence::ps2::spBaseObjectLayout base; // 0x00
        std::uint32_t stateSelector;                       // 0x10
        Address32 owner;                                   // 0x14
        Address32 completionConsumer;                      // 0x18
        std::uint8_t transitionFlag1C;                     // 0x1c
        std::uint8_t transitionFlag1D;                     // 0x1d
        std::uint8_t transitionFlag1E;                     // 0x1e
        std::uint8_t transitionFlag1F;                     // 0x1f
        std::uint8_t transitionFlag20;                     // 0x20
        std::uint8_t padding21[3];                         // 0x21
        Address32 pendingHandle;                           // 0x24
        Address32 field28;                                 // 0x28
        std::uint8_t field2C;                              // 0x2c
        std::uint8_t padding2D[3];                         // 0x2d
        std::uint32_t resetValues[3];                      // 0x30
    };

    static_assert(sizeof(wxCharacterStateLayout) == 0x3C);
    static_assert(offsetof(wxCharacterStateLayout, stateSelector) == 0x10);
    static_assert(offsetof(wxCharacterStateLayout, pendingHandle) == 0x24);
    static_assert(offsetof(wxCharacterStateLayout, resetValues) == 0x30);

    inline constexpr std::uint32_t wxCharacterStateClassID = 0x44817BC2;
    inline constexpr std::uint32_t wxActionStateClassID = 0x196333F8;
    inline constexpr Address32 wxCharacterStateFactory = 0x003F52A0;
    inline constexpr Address32 wxCharacterStateConstructor = 0x002C9150;
    inline constexpr Address32 wxCharacterStateVTable = 0x0049A9E0;
    inline constexpr Address32 wxActionStateFactory = 0x003F33A0;
    inline constexpr Address32 wxActionStateConstructor = 0x002C6CD0;
    inline constexpr Address32 wxActionStateVTable = 0x0049A080;
    inline constexpr Address32 wxActionStateSlot1C = 0x002C6A00;
    inline constexpr Address32 wxActionStateSlot20 = 0x002C68A0;
    inline constexpr Address32 wxActionStateSlot30 = 0x002C6B50;
}
