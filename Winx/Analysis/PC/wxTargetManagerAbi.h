#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace winx::evidence::pc
{
    struct wxTargetManagerLayout final
    {
        std::array<std::byte, 0x10> base;
        std::uint32_t singletonVTable10, listAllocator14, listSentinel18, listCount1C;
        std::uint32_t current20, current24, current28;
        std::uint8_t flag2C, unwritten2D[3];
        std::uint32_t referencedNode30;
        std::uint8_t flag34, unwritten35[3];
        std::uint32_t owned38, word3C, referenced40, owned44, word48, owned4C;
        std::uint8_t flag50, unwritten51[3];
    };
    static_assert(sizeof(wxTargetManagerLayout) == 0x54);
    static_assert(offsetof(wxTargetManagerLayout, referencedNode30) == 0x30);
    static_assert(offsetof(wxTargetManagerLayout, flag50) == 0x50);
    inline constexpr std::uint32_t wxTargetManagerFactory = 0x0059E090;
    inline constexpr std::uint32_t wxTargetManagerConstructor = 0x0059DFE0;
    inline constexpr std::uint32_t wxTargetManagerVTable = 0x00703954;
    inline constexpr std::uint32_t wxTargetManagerRTTI = 0x00766290;
    inline constexpr std::uint32_t wxTargetManagerSingleton = 0x00765AF0;
    inline constexpr std::uint32_t wxTargetManagerResourceInitialize = 0x0059CCC0;
    inline constexpr std::uint32_t wxTargetManagerReset = 0x0059D030;
    inline constexpr std::uint32_t wxTargetManagerDestructor = 0x0059DC40;
    inline constexpr std::uint32_t wxTargetManagerDeletingDestructor = 0x0059DF50;
    inline constexpr std::uint32_t wxTargetManagerClone = 0x0059E0F0;
}
