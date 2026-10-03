#pragma once
// Observed original PC layout, independent of the host C++ ABI.
#include "spNetworkAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spDXNetworkLayout final
    {
        spNetworkObservedPrefixLayout base;
        std::uint32_t socket;                    // +28: INVALID_SOCKET
        std::uint8_t socketAddress[16];          // +2C: constructor does not write
        Address32 ownedLocalAddressText;         // +3C: null, lazy16-byte allocation
    };
    static_assert(sizeof(spDXNetworkLayout)==0x40);
    static_assert(offsetof(spDXNetworkLayout,socket)==0x28);
    static_assert(offsetof(spDXNetworkLayout,socketAddress)==0x2c);
    static_assert(offsetof(spDXNetworkLayout,ownedLocalAddressText)==0x3c);
    inline constexpr Address32 DXNetworkPrimaryTable=0x006ef928;
    inline constexpr Address32 DXNetworkObjectTable=0x006ef90c;
    inline constexpr std::array<Address32,17> DXNetworkPrimaryMethods{
        0x004ac960,0x004ac9b0,0x004aca30,0x004acae0,0x004ad180,
        0x004acb00,0x004acb20,0x004ace60,0x004acee0,0x004ad010,
        0x004ad250,0x004acb60,0x004acb90,0x004acbf0,0x004acc20,
        0x004acc60,0x004ace30};
}
