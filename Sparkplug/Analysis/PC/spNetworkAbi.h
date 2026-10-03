#pragma once
// Original PC interface tables and observed constructor extent. No independent
// base allocation exists, so 28h is an observed prefix, not proven sizeof.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spNetworkObservedPrefixLayout final
    {
        Address32 networkVtable;                 // +00: primary network interface
        spCrossPlatformLayout objectInterface;   // +04: secondary named object
        std::uint8_t connected;                  // +18: constructor0
        std::uint8_t padding19[3];
        std::uint32_t type;                      // +1C: constructor1; 0UDP/1TCP in DX
        std::uint32_t address;                   // +20: constructorFFFFFFFF
        std::uint16_t port;                      // +24: constructorFFFF
        std::uint8_t padding26[2];
    };
    static_assert(sizeof(spNetworkObservedPrefixLayout)==0x28);
    static_assert(offsetof(spNetworkObservedPrefixLayout,objectInterface)==4);
    static_assert(offsetof(spNetworkObservedPrefixLayout,connected)==0x18);
    static_assert(offsetof(spNetworkObservedPrefixLayout,type)==0x1c);
    static_assert(offsetof(spNetworkObservedPrefixLayout,address)==0x20);
    static_assert(offsetof(spNetworkObservedPrefixLayout,port)==0x24);

    inline constexpr Address32 NetworkPrimaryTable=0x006eee50;
    inline constexpr Address32 NetworkObjectTable=0x006eee30;
    inline constexpr Address32 NetworkRecord=0x00762f30;
    inline constexpr Address32 NetworkConstructor=0x004a1c40;
    inline constexpr Address32 NetworkDestructor=0x004a1c00;
    inline constexpr Address32 NetworkSecondaryDelete=0x004a1c30;
    inline constexpr Address32 NetworkCompleteDelete=0x004a1c80;
    inline constexpr Address32 NetworkNullReturn=0x004a1bf0;
    // Actual primary16 slots point at purecall; the final getter is null.
    inline constexpr std::array<Address32,17> NetworkPrimaryMethods{
        0x0060db76,0x0060db76,0x0060db76,0x0060db76,
        0x0060db76,0x0060db76,0x0060db76,0x0060db76,
        0x0060db76,0x0060db76,0x0060db76,0x0060db76,
        0x0060db76,0x0060db76,0x0060db76,0x0060db76,0x004a1bf0};
    // PC secondary offsets00/04/08/0C/10/14/18, not portable method offsets.
    inline constexpr std::array<Address32,7> NetworkObjectMethods{
        0x004a1c30,0x005b7a00,0x004a1bf0,0x00413120,
        0x004a1c20,0x00408350,0x00408370};
}
