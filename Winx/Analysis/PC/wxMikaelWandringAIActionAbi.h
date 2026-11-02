#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxMikaelWandringAIActionLayout final
    {
        wxAIActionLayout base; // borrowed command is base.tail3A4, not own3A8.
        std::uint8_t rawOwnBytes3A8[8];
        Address32 borrowedSourceNode3B0;
        Address32 borrowedTargetNode3B4;
        std::uint8_t rawOwnBytes3B8[48];
    };
    static_assert(sizeof(wxMikaelWandringAIActionLayout) == 0x3E8);
    static_assert(offsetof(wxAIActionLayout, tail3A4) == 0x3A4);
    static_assert(offsetof(wxMikaelWandringAIActionLayout, rawOwnBytes3A8) == 0x3A8);
    static_assert(offsetof(wxMikaelWandringAIActionLayout, borrowedSourceNode3B0) == 0x3B0);
    static_assert(offsetof(wxMikaelWandringAIActionLayout, borrowedTargetNode3B4) == 0x3B4);
    static_assert(offsetof(wxMikaelWandringAIActionLayout, rawOwnBytes3B8) == 0x3B8);
    inline constexpr Address32 wxMikaelWandringAIActionFactory = 0x5C95B0;
    inline constexpr Address32 wxMikaelWandringAIActionConstructor = 0x5C90D0;
    inline constexpr Address32 wxMikaelWandringAIActionGetter = 0x5C8F00;
    inline constexpr Address32 wxMikaelWandringAIActionRTTI = 0x767AF0;
    inline constexpr Address32 wxMikaelWandringAIActionVTable = 0x70A608;
    inline constexpr Address32 wxMikaelWandringAIActionEntryCell = 0x70A630;
    inline constexpr Address32 wxMikaelWandringAIActionEntry = 0x5C8E10;
    inline constexpr Address32 wxMikaelWandringAIActionOwn48Cell = 0x70A650;
    inline constexpr Address32 wxMikaelWandringAIActionOwn48 = 0x5C8F50;
    inline constexpr Address32 wxMikaelWandringAIActionOwn4CCell = 0x70A654;
    inline constexpr Address32 wxMikaelWandringAIActionOwn4C = 0x5C9020;
    inline constexpr std::size_t wxMikaelWandringAIActionStoredPointOffset = 0x3B8;
}
