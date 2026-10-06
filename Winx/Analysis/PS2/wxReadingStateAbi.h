#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxReadingStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxReadingStateLayout) == 0x3c);
    inline constexpr Address32 ReadingTable = 0x00499bd0;
    inline constexpr Address32 ReadingFactory = 0x003f24a0;
    inline constexpr Address32 ReadingClone = 0x003f23e0;
    inline constexpr Address32 ReadingConstructor = 0x002d2c10;
    inline constexpr Address32 ReadingEntry = 0x002d28c0;
    inline constexpr Address32 ReadingExit = 0x002d2660;
    inline constexpr Address32 ReadingUpdate = 0x002d2a30;
    inline constexpr Address32 ReadingPermission = 0x002d2b60;
}
