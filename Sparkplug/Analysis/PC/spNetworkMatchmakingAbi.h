#pragma once
// Original PC storage; independent of the portable C++ ABI.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spNetworkMatchmakingLayout final
    {
        spBaseObjectLayout base;
        Address32 socketStream;                    // +10, owned
        Address32 memoryStream;                    // +14, owned
        std::uint32_t stringAllocatorWord;          // +18, untouched
        std::uint8_t stringStorage[16];             // +1C, initially first byte0
        std::uint32_t stringSize,stringCapacity;    // +2C,+30
    };
    static_assert(sizeof(spNetworkMatchmakingLayout)==0x34);
    static_assert(offsetof(spNetworkMatchmakingLayout,socketStream)==0x10);
    static_assert(offsetof(spNetworkMatchmakingLayout,memoryStream)==0x14);
    static_assert(offsetof(spNetworkMatchmakingLayout,stringSize)==0x2c);
    inline constexpr Address32 NetworkMatchmakingTable=0x006ebcd0;
    inline constexpr Address32 NetworkMatchmakingRecord=0x00761638;
    inline constexpr Address32 NetworkMatchmakingConstructor=0x00482470; // protected013D2F60
    inline constexpr Address32 NetworkMatchmakingFactory=0x00482730;
    inline constexpr Address32 NetworkMatchmakingDestructor=0x00482390;
    inline constexpr Address32 NetworkMatchmakingClose=0x00482340;
    inline constexpr Address32 NetworkMatchmakingPump=0x00482510;
    inline constexpr std::array<Address32,7> NetworkMatchmakingObjectSlots{
        0x00482450,0x005b7a00,0x00482790,0x0040ece0,
        0x00482440,0x00408350,0x00408370};
}
