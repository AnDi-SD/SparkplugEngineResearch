#pragma once

// Original PC32 object/table ABI, kept separate from portable C++ storage.
#include "SparkBaseAbi.h"
#include <array>

namespace sparkplug::evidence::pc
{
    struct spTextureManagerRecordLayout final
    {
        std::uint32_t unknownWords[3];
    };
    struct spTextureManagerLayout final
    {
        spBaseObjectLayout base;               // +00
        Address32 singletonSupportVtable;      // +10
        std::uint8_t reserveFlag;              // +14, raw byte
        std::uint8_t unwrittenPadding15[3];
        std::uint32_t requestedRecordCount;    // +18
        std::uint32_t unwrittenAllocatorWord;  // +1C
        Address32 begin, end, capacityEnd;      // +20/+24/+28
    };
    static_assert(sizeof(spTextureManagerRecordLayout) == 0x0C);
    static_assert(sizeof(spTextureManagerLayout) == 0x2C);
    static_assert(offsetof(spTextureManagerLayout, singletonSupportVtable) == 0x10);
    static_assert(offsetof(spTextureManagerLayout, reserveFlag) == 0x14);
    static_assert(offsetof(spTextureManagerLayout, requestedRecordCount) == 0x18);
    static_assert(offsetof(spTextureManagerLayout, begin) == 0x20);

    inline constexpr Address32 TextureManagerSingleton = 0x00764E8C;
    inline constexpr Address32 TextureManagerConstructor = 0x005D65E0;
    inline constexpr Address32 TextureManagerDestructor = 0x005D6540;
    inline constexpr Address32 TextureManagerConfigureReserve = 0x005D65C0;
    inline constexpr Address32 TextureManagerReserve = 0x005D6440;
    inline constexpr Address32 TextureManagerVtable = 0x0070EDA4;
    inline constexpr Address32 DXTextureManagerVtable = 0x0070AC94;
    inline constexpr Address32 DXTextureManagerSupportVtable = 0x0070AC90;
    inline constexpr Address32 DXTextureManagerFactory = 0x005CF100;
    inline constexpr Address32 DXTextureManagerClone = 0x005CF170;
    inline constexpr Address32 DXTextureManagerDeletingDestructor = 0x005CF1C0;
    inline constexpr Address32 DXTextureManagerSupportDeletingAdjustor = 0x005CF0F0;
    inline constexpr std::array<Address32, 9> DXTextureManagerObjectSlots{
        0x005CF1C0, 0x005B7A00, 0x005CF170, 0x0040ECE0,
        0x005CF0C0, 0x00408350, 0x00408370, 0x0048EAA0, 0x0048EAA0,
    };
}
