#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Prefix offsets observed inside actual DX instances, not a claim about
    // an independent complete-object allocation size for the abstract base.
    struct spAudioBankEntryPrefix32 final
    {
        std::array<std::byte,0x10> base;
        std::uint32_t name10;
        std::uint8_t rawByte14;
        std::array<std::byte,3> unassigned15;
        std::uint32_t rawWord18,rawWord1C,rawWord20,rawWord24;
        std::uint8_t rawByte28;
        std::array<std::byte,3> unassigned29;
        std::uint32_t rawWord2C;
        std::uint8_t rawByte30;
        std::array<std::byte,3> unassigned31;
    };
    struct spDXAudioBankEntryLayout final
    {
        spAudioBankEntryPrefix32 base;
        std::array<std::uint32_t,11> rawWords34To5C;
        std::array<std::byte,4> unassigned60;
        std::uint32_t rawWord64,rawWord68;
        std::array<std::byte,0x28> unassigned6C;
        std::uint32_t rawWord94;
        std::array<std::byte,0x48> unassigned98;
        std::array<std::byte,4> unassignedE0;
        std::uint32_t rawWordE4;
        std::array<std::byte,0xC> unassignedE8;
        std::uint32_t rawWordF4;
        std::array<std::byte,0xC> unassignedF8;
    };
    static_assert(sizeof(spAudioBankEntryPrefix32)==0x34);
    static_assert(offsetof(spAudioBankEntryPrefix32,rawByte30)==0x30);
    static_assert(sizeof(spDXAudioBankEntryLayout)==0x104);
    static_assert(offsetof(spDXAudioBankEntryLayout,unassigned98)==0x98);
    static_assert(offsetof(spDXAudioBankEntryLayout,unassignedE0)==0xE0);
    static_assert(offsetof(spDXAudioBankEntryLayout,rawWordF4)==0xF4);
    inline constexpr std::uint32_t spAudioBankEntryConstructor=0x4CF0E0;
    inline constexpr std::uint32_t spAudioBankEntryDestructor=0x4CF120;
    inline constexpr std::uint32_t spAudioBankEntryTable=0x6F35D4;
    inline constexpr std::uint32_t spDXAudioBankEntryFactory=0x4C7950;
    inline constexpr std::uint32_t spDXAudioBankEntryConstructor=0x4C7560;
    inline constexpr std::uint32_t spDXAudioBankEntryClone=0x4C79B0;
    inline constexpr std::uint32_t spDXAudioBankEntryDelete=0x4C7A00;
    inline constexpr std::uint32_t spDXAudioBankEntryTable=0x6F2B34;
    inline constexpr std::uint32_t spAudioBankEntryNameCopy=0x413120;
}
