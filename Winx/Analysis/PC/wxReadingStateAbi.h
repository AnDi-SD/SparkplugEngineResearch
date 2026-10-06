#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxReadingStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxReadingStateLayout) == 0x3c);
    inline constexpr Address32 ReadingTable = 0x006f92a0;
    inline constexpr Address32 ReadingFactory = 0x00403500;
    inline constexpr Address32 ReadingClone = 0x00409ed0;
    inline constexpr Address32 ReadingEntry = 0x0051aae0;
    inline constexpr Address32 ReadingExit = 0x0051ab90;
    inline constexpr Address32 ReadingUpdate = 0x0051aa80;
    inline constexpr Address32 ReadingPermission = 0x0051aa60;
}
