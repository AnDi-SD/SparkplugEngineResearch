#pragma once
// Original32-bit PC storage. Portable class storage/layout is independent.
#include "SparkBaseAbi.h"

namespace sparkplug::evidence::pc
{
    struct spNetworkPeerLayout final
    {
        spBaseObjectLayout base;
        std::uint16_t id;                    // +10, BAD0
        std::uint8_t byte12,unknown13;        // +12 adoption; +13 Server Ready producer
        Address32 tcp,udp;                    // +14,+18, owned SocketStreams
        std::uint32_t timestamp;              // +1C, external manager writes
        Address32 buffer;                     // +20, owned MemoryStream
    };
    static_assert(sizeof(spNetworkPeerLayout)==0x24);
    static_assert(offsetof(spNetworkPeerLayout,tcp)==0x14);
    static_assert(offsetof(spNetworkPeerLayout,buffer)==0x20);
    inline constexpr Address32 NetworkPeerTable=0x006ebd7c;
    inline constexpr Address32 NetworkPeerRecord=0x007616f8;
    inline constexpr Address32 NetworkPeerConstructor=0x00482d20;
    inline constexpr Address32 NetworkPeerDestructor=0x00482db0;
    inline constexpr Address32 NetworkPeerAdoptTcp=0x00482e40;
    inline constexpr Address32 NetworkPeerClose=0x00482e90;
    inline constexpr Address32 NetworkPeerFactory=0x00482eb0;
    inline constexpr Address32 NetworkPeerClone=0x00482f10;
    inline constexpr Address32 NetworkPeerDelete=0x00482f60;
    inline constexpr Address32 NetworkPeerOpen=0x00482f80;
    inline constexpr Address32 NetworkPeerSendPacket=0x00482ff0;
    inline constexpr Address32 NetworkPeerSend=0x004830a0;
}
