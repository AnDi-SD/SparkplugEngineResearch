#pragma once
#include "SparkplugAbi.h"

namespace sparkplug::evidence::pc
{
    // Existing byte-exact24 layout is spPCProjectionManagerLayout. The base
    // constructor uses the same extent; no factory exists for its RTTI record.
    inline constexpr Address32 spProjectionManagerConstructor=0x004CDFB0;
    inline constexpr Address32 spProjectionManagerVTable=0x006F34B0;
    inline constexpr Address32 spProjectionManagerRTTI=0x00765318;
    inline constexpr Address32 spPCProjectionManagerFactory=0x004C5CE0;
    inline constexpr Address32 spPCProjectionManagerVTable=0x006F2A00;
    inline constexpr Address32 spPCProjectionManagerRTTI=0x00764AD0;
    inline constexpr Address32 spProjectionManagerInitialize=0x004CDE70;
    inline constexpr Address32 spProjectionManagerAdd=0x004CDF60;
    inline constexpr Address32 spProjectionManagerRemove=0x0045A3D0;
    inline constexpr Address32 spProjectionManagerDraw=0x004CDEA0;
    inline constexpr Address32 spProjectionInitializeHelper=0x004243D0;
}
