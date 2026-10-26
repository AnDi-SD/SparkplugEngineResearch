#pragma once
// Exact shipped PC storage. This is evidence, not the portable object ABI.
#include "SparkBaseAbi.h"

namespace sparkplug::evidence::pc
{
    struct spNetworkServerListLayout final
    {
        std::uint32_t allocatorWord; // Not written by constructor.
        Address32 sentinel;
        std::uint32_t count;
    };
    struct spNetworkServerLayout final
    {
        spBaseObjectLayout base;
        Address32 category;                    // +10, string, not vtable.
        std::uint16_t id,unknown16;             // +14 BAD0; +16 unwritten.
        spNetworkServerListLayout availableIds; // +18, uint16 node values.
        Address32 thread,tcp,udp;               // +24,+28,+2C, owned.
        Address32 peers;                       // +30, owned list container.
        Address32 peerLock,queue,buffer;        // +34,+38,+3C.
        std::uint32_t acceptedCount;            // +40, initially zero.
    };
    static_assert(sizeof(spNetworkServerLayout)==0x44);
    static_assert(offsetof(spNetworkServerLayout,thread)==0x24);
    static_assert(offsetof(spNetworkServerLayout,queue)==0x38);
    inline constexpr Address32 NetworkServerTable=0x006ec04c;
    inline constexpr Address32 NetworkServerRecord=0x00761758;
    inline constexpr Address32 NetworkServerConstructor=0x00483ce0;
    inline constexpr Address32 NetworkServerDestructor=0x00484340;
    inline constexpr Address32 NetworkServerFactory=0x00484290;
    inline constexpr Address32 NetworkServerClone=0x004842f0;
    inline constexpr Address32 NetworkServerDelete=0x004847a0;
    inline constexpr Address32 NetworkServerOpen=0x00484820;
    inline constexpr Address32 NetworkServerClose=0x00483de0;
    inline constexpr Address32 NetworkServerSetQueue=0x004847c0;
    inline constexpr Address32 NetworkServerSetPeer=0x004838e0;
    inline constexpr Address32 NetworkServerFindPeer=0x004833c0;
    inline constexpr Address32 NetworkServerCountOwned=0x00483480;
    inline constexpr Address32 NetworkServerPrune=0x00483520;
    inline constexpr Address32 NetworkServerBroadcast=0x00483360;
    inline constexpr Address32 NetworkServerReleaseId=0x00483c20;
    inline constexpr Address32 NetworkServerInitialReply=0x00483110;
    inline constexpr Address32 NetworkServerInitialRequest=0x00483a60;
    inline constexpr Address32 NetworkServerAllocateId=0x00483850;
    inline constexpr Address32 NetworkServerAllocateIdBody=0x00492d90;
    inline constexpr Address32 NetworkServerReady=0x00483610;
    inline constexpr Address32 NetworkServerHello=0x00483740;
    inline constexpr Address32 NetworkServerNewTcpConnection=0x00483930;
    inline constexpr Address32 NetworkServerProcessPackets=0x00483f30;
    inline constexpr Address32 NetworkServerWorker=0x00484520;
    inline constexpr Address32 NetworkServerCallback=0x00484800;
}
