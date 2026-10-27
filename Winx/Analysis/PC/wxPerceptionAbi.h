#pragma once

#include "Analysis/PC/SparkBaseAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxPerceptionLayout final
    {
        sparkplug::evidence::pc::spBaseObjectLayout base;
        std::uint32_t owner; // +10
        std::uint8_t nativeTargetList[0x0C]; // +14, sentinel pointer at +18
        std::uint32_t word20;
        std::uint32_t words24[3];
        std::uint32_t scratchWords30[3]; // unwritten by constructor
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
    static_assert(offsetof(wxPerceptionLayout, word74) == 0x74);
    inline constexpr std::uint32_t wxPerceptionFactory = 0x4F2020;
    inline constexpr std::uint32_t wxPerceptionConstructor = 0x4F1E80;
    inline constexpr std::uint32_t wxPerceptionVtable = 0x6F6288;
    inline constexpr std::uint32_t wxPerceptionRTTIGetter = 0x4F1C40;
    inline constexpr std::uint32_t wxPerceptionClone = 0x4F2080;
    inline constexpr std::uint32_t wxPerceptionCopy = 0x4F20D0;
    inline constexpr std::uint32_t wxPerceptionReset = 0x4F18E0;
    inline constexpr std::uint32_t wxPerceptionFindNearest = 0x4F1910;
    inline constexpr std::uint32_t wxPerceptionRefresh = 0x4F1F30;
    inline constexpr std::uint32_t wxPerceptionOwnerScan = 0x4F1C70;
    inline constexpr std::uint32_t wxPerceptionOwnerScanBoundary = 0x4F1C8A;
}
