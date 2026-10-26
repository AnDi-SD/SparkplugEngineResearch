#pragma once
// Shipped32-bit PC layout. Unknown words/padding are preserved as unknown;
// this is evidence storage, not the portable C++ object's layout.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spNetworkManagerStringLayout final
    {
        std::uint32_t allocatorWord;
        std::uint8_t storage[16];
        std::uint32_t size,capacity;
    };
    struct spNetworkManagerLayout final
    {
        spBaseObjectLayout base;                    // +00
        Address32 singletonInterface;               // +10
        std::uint8_t started,word15,forward,padding17;
        Address32 category,server,peer,matchmaking;  // +18..24
        Address32 queueLock;                        // +28
        std::uint32_t queueAllocatorWord;           // +2C untouched
        Address32 queueSentinel;                    // +30
        std::uint32_t queueCount;                   // +34
        std::uint8_t unknown38,padding39[3];
        std::uint32_t words3C[4];                   // +3C..48
        std::uint32_t unknown4C;                     // +4C, no established recipient role
        std::uint32_t unknown50;
        std::uint8_t unknown54,padding55[3];
        std::uint32_t words58[6];                   // +58..6C
        Address32 debug,currentPacket,controller,statistics;
        std::int32_t error;                         // +80
        spNetworkManagerStringLayout errors[10];    // +84..19B
        Address32 errorLock,timer;                  // +19C,+1A0
        std::uint32_t bias,unknown1A8;
        std::uint16_t uniqueId;
        std::uint8_t padding1AE[2];
        spNetworkManagerStringLayout text;          // +1B0
    };
    static_assert(sizeof(spNetworkManagerStringLayout)==0x1c);
    static_assert(sizeof(spNetworkManagerLayout)==0x1cc);
    static_assert(offsetof(spNetworkManagerLayout,queueLock)==0x28);
    static_assert(offsetof(spNetworkManagerLayout,unknown4C)==0x4c);
    static_assert(offsetof(spNetworkManagerLayout,debug)==0x70);
    static_assert(offsetof(spNetworkManagerLayout,errors)==0x84);
    static_assert(offsetof(spNetworkManagerLayout,errorLock)==0x19c);
    static_assert(offsetof(spNetworkManagerLayout,uniqueId)==0x1ac);
    static_assert(offsetof(spNetworkManagerLayout,text)==0x1b0);
    inline constexpr Address32 NetworkManagerTable=0x006e6b3c;
    inline constexpr Address32 NetworkManagerRecord=0x0075f6f8;
    inline constexpr Address32 NetworkManagerFactory=0x004512d0;
    inline constexpr Address32 NetworkManagerConstructor=0x00450f40; // protected13CB860
    inline constexpr Address32 NetworkManagerDestructor=0x004511e0; // protected13C6BB0
    inline constexpr Address32 NetworkManagerSingleton=0x0075db70;
    inline constexpr Address32 NetworkManagerNotifyObservers=0x00450b70;
    inline constexpr Address32 NetworkManagerSendReady=0x004516f0;
    inline constexpr std::array<Address32,9> NetworkManagerMethods{
        0x00451380,0x00450b40,0x00451330,0x0040ece0,0x004511c0,
        0x00408350,0x00408370,0x004509b0,0x00450dc0};
}
