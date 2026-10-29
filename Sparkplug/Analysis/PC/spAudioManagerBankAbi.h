#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Complete DX allocation extent from actual4C4200. Unknown prefix/tail
    // bytes are opaque; this does not assign a complete base-class size or
    // a C++ type to the observed secondary interface word10.
    struct spDXAudioManagerBankRegistryLayout final
    {
        std::uint32_t primaryVtable;
        std::array<std::byte,0x0C> base04;
        std::uint32_t secondaryInterface10;
        std::array<std::byte,0x6C> opaque14;
        std::array<std::byte,4> unassigned80;
        std::uint32_t banksBegin84;
        std::uint32_t banksEnd88;
        std::uint32_t banksLimit8C;
        std::array<std::byte,0xC8> opaque90;
    };
    static_assert(sizeof(spDXAudioManagerBankRegistryLayout)==0x158);
    static_assert(offsetof(spDXAudioManagerBankRegistryLayout,banksBegin84)==0x84);
    static_assert(offsetof(spDXAudioManagerBankRegistryLayout,banksEnd88)==0x88);
    inline constexpr std::uint32_t spDXAudioManagerFactory=0x4C4200;
    inline constexpr std::uint32_t spAudioManagerRemoveBank=0x48F9B0;
    inline constexpr std::uint32_t spAudioManagerRemoveBankSlot=0x38;
    inline constexpr std::uint32_t spAudioManagerPrimaryTable=0x6EC620;
    inline constexpr std::uint32_t spDXAudioManagerPrimaryTable=0x6F26D8;
}
