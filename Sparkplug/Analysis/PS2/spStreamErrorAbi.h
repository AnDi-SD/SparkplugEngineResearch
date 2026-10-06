#pragma once
#include "SparkBaseAbi.h"
namespace sparkplug::evidence::ps2
{
    using spStreamErrorLayout = spErrorLayout;
    static_assert(sizeof(spStreamErrorLayout) == 0x28);
    inline constexpr Address32 spStreamErrorVTable = 0x0048C770;
    inline constexpr Address32 spStreamErrorRegistration = 0x004A0D20;
    inline constexpr Address32 spStreamErrorDescribe = 0x00107DB0;
    inline constexpr Address32 spStreamErrorFactory = 0x001082F0;
    inline constexpr Address32 spStreamErrorClone = 0x00108220;
}
