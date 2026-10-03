#pragma once
// Original PC storage, independent of the portable C++ ABI.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spNetworkStateCtrlLayout final
    {
        spBaseObjectLayout base;
        Address32 lock;                        // +10, owned storage
        std::uint8_t changed,padding15[3];      // +14; padding untouched
        std::uint32_t target,previous,current; // +18,+1C,+20
        std::uint32_t listAllocatorWord;       // +24, untouched
        Address32 queueHead;                   // +28, owned sentinel
        std::uint32_t queued,result;           // +2C,+30
    };
    static_assert(sizeof(spNetworkStateCtrlLayout)==0x34);
    static_assert(offsetof(spNetworkStateCtrlLayout,lock)==0x10);
    static_assert(offsetof(spNetworkStateCtrlLayout,changed)==0x14);
    static_assert(offsetof(spNetworkStateCtrlLayout,current)==0x20);
    static_assert(offsetof(spNetworkStateCtrlLayout,result)==0x30);
    inline constexpr Address32 NetworkStateCtrlTable=0x006ebd3c;
    inline constexpr Address32 NetworkStateCtrlRecord=0x00761698;
    inline constexpr Address32 NetworkStateCtrlConstructor=0x00482bd0; // protected013E2950
    inline constexpr Address32 NetworkStateCtrlFactory=0x00482c70;
    inline constexpr Address32 NetworkStateCtrlDestructor=0x00482a40;
    inline constexpr Address32 NetworkStateCtrlRequest=0x00482a90;
    inline constexpr Address32 NetworkStateCtrlApplyImmediate=0x00482920; // protected013C7000
    inline constexpr Address32 NetworkStateCtrlPump=0x00482b10;
    inline constexpr Address32 NetworkStateCtrlEvaluate=0x004827e0; // protected route to004F5D30
    inline constexpr Address32 NetworkStateCtrlEvaluateBody=0x004f5d30;
    inline constexpr std::array<Address32,7> NetworkStateCtrlObjectSlots{
        0x00482c50,0x005b7a00,0x00482cd0,0x0040ece0,
        0x00482a80,0x00408350,0x00408370};
}
