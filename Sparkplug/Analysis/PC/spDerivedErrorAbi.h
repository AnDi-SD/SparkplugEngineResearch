#pragma once
#include "SparkBaseAbi.h"
namespace sparkplug::evidence::pc
{
    using spStreamErrorLayout = spErrorObservedPrefixLayout;
    using spWindowsErrorLayout = spErrorObservedPrefixLayout;
    static_assert(sizeof(spStreamErrorLayout) == 0x28);
    static_assert(sizeof(spWindowsErrorLayout) == 0x28);
    inline constexpr Address32 spStreamErrorVTable = 0x006EC6E4;
    inline constexpr Address32 spStreamErrorRegistration = 0x007628F0;
    inline constexpr Address32 spStreamErrorDescribe = 0x004905E0;
    inline constexpr Address32 spStreamErrorFactory = 0x00490A40;
    inline constexpr Address32 spStreamErrorClone = 0x00490AB0;
    inline constexpr Address32 spWindowsErrorVTable = 0x007292C4;
    inline constexpr Address32 spWindowsErrorRegistration = 0x0084D6F0;
    inline constexpr Address32 spWindowsErrorDescribe = 0x006BE830;
    inline constexpr Address32 spWindowsErrorFactory = 0x006BE8F0;
    inline constexpr Address32 spWindowsErrorClone = 0x006BE960;
}
