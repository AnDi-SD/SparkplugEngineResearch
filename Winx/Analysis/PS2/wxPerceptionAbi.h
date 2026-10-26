#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    // Native offsets after the common sixteen-byte base match PC, while the
    // compiler list has an embedded sentinel at +18 and forward link at +4.
    struct wxPerceptionLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t owner;
        std::uint8_t nativeTargetList[0x0C];
        std::uint32_t word20;
        std::uint32_t words24[3];
        std::uint32_t scratchWords30[3];
        std::uint32_t word3C;
        std::uint8_t byte40, unknown41[3];
        std::uint32_t word44;
        std::uint8_t byte48, unknown49[3];
        std::uint32_t word4C, word50, word54, word58, word5C, word60;
        std::uint8_t byte64, byte65, unknown66[2];
        std::uint32_t opaque68, opaque6C;
        std::uint8_t byte70, unknown71[3];
        std::uint32_t word74;
        std::uint8_t byte78, unknown79[3];
    };
    static_assert(sizeof(wxPerceptionLayout) == 0x7C);
    static_assert(offsetof(wxPerceptionLayout, scratchWords30) == 0x30);
    inline constexpr std::uint32_t wxPerceptionFactory = 0x36F800;
    inline constexpr std::uint32_t wxPerceptionConstructor = 0x36F670;
    inline constexpr std::uint32_t wxPerceptionVtable = 0x494640;
    inline constexpr std::uint32_t wxPerceptionClone = 0x36F740;
    inline constexpr std::uint32_t wxPerceptionCopy = 0x36ED40;
    inline constexpr std::uint32_t wxPerceptionFindNearest = 0x36EBC0;
}
