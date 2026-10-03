#pragma once
#include <cstddef>
#include <cstdint>
namespace sparkplug::reconstruction::pc
{
    // Evidence metadata, not a native object overlay or host ABI claim.
    struct spSocketStreamAbi
    {
        static constexpr std::size_t Size=0x30,Network=0x1c,Type=0x20,
            Address=0x24,PortWord=0x28,Blocking=0x2c;
        static constexpr std::uint32_t Vtable=0x6ed0a8,Factory=0x499410,
            Constructor=0x4991e0,Destructor=0x499250,Clone=0x499470,
            Open=0x4992b0,Close=0x499330,ReceiveStream=0x499340,
            WriteData=0x4993b0,SendStream=0x4993d0,Adopt=0x4994e0,
            Accept=0x499580;
    };
}
