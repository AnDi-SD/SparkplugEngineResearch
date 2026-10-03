#pragma once
#include <cstddef>
#include <cstdint>
namespace sparkplug::reconstruction::pc
{
    struct spNetworkPacketAbi
    {
        static constexpr std::size_t Size=0x28,Source=0x10,Destination=0x12,
            PacketType=0x14,UseTcp=0x16,DataField1=0x18,DataField2=0x1c,
            PayloadSize=0x20,Payload=0x24,PayloadCapacity=256,WireHeaderSize=17;
        static constexpr std::uint32_t Factory=0x499960,Constructor=0x4995b0,
            Clone=0x4999c0,Destructor=0x499630,Read=0x4996d0,Write=0x499850;
    };
}
