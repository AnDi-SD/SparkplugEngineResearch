#pragma once
// Shipped PC byte layout, separate from portable host C++ layout.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spNetworkDebugTimerLayout final
    {
        spBaseObjectLayout base;
        std::uint8_t active,padding11[3];
        std::uint32_t accumulated,start;
        std::uint8_t limited,padding1D[3];
        std::uint32_t limit;
    };
    struct spNetworkDebugLayout final
    {
        spBaseObjectLayout base;
        std::uint8_t consoleEnabled,fileEnabled,unknown12,subscribed;
        Address32 stream,lock;
        std::uint32_t stringAllocatorWord; // +1C untouched by constructor
        std::uint8_t stringStorage[16];    // +20 SSO bytes or heap pointer
        std::uint32_t stringSize,stringCapacity;
        spNetworkDebugTimerLayout timer;
        Address32 context;
        std::int32_t level;
    };
    static_assert(sizeof(spNetworkDebugTimerLayout)==0x24);
    static_assert(sizeof(spNetworkDebugLayout)==0x64);
    static_assert(offsetof(spNetworkDebugLayout,stream)==0x14);
    static_assert(offsetof(spNetworkDebugLayout,lock)==0x18);
    static_assert(offsetof(spNetworkDebugLayout,stringSize)==0x30);
    static_assert(offsetof(spNetworkDebugLayout,timer)==0x38);
    static_assert(offsetof(spNetworkDebugLayout,context)==0x5c);
    static_assert(offsetof(spNetworkDebugLayout,level)==0x60);
    inline constexpr Address32 NetworkDebugTable=0x006ebbb8;
    inline constexpr Address32 NetworkDebugRecord=0x007615d8;
    inline constexpr Address32 NetworkDebugConstructor=0x00481c70; // resolved4C9B20
    inline constexpr Address32 NetworkDebugFactory=0x00481df0;
    inline constexpr Address32 NetworkDebugDestructor=0x00481d30;
    inline constexpr Address32 NetworkDebugStart=0x00481a00;
    inline constexpr Address32 NetworkDebugStop=0x00481a20;
    inline constexpr Address32 NetworkDebugSetFile=0x00481a90;
    inline constexpr Address32 NetworkDebugRender=0x00481ba0;
    inline constexpr Address32 NetworkDebugLog=0x00481ec0;
    inline constexpr Address32 NetworkDebugDumpPacket=0x004820a0;
    inline constexpr std::array<Address32,9> NetworkDebugMethods{
        0x00481ea0,0x00481dd0,0x00481e50,0x0040ece0,0x00481d20,
        0x00408350,0x00408370,0x00481a00,0x00481a20};
}
