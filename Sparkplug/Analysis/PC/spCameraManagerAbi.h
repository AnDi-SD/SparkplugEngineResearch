#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct spCameraManagerLayout final
    {
        std::array<std::byte,0x10> base;
        std::uint32_t secondaryTable10;
        std::uint32_t field14;
        std::array<std::byte,4> unassigned18;
        std::uint32_t wordsBegin1C;
        std::uint32_t wordsEnd20;
        std::uint32_t wordsLimit24;
    };
    static_assert(sizeof(spCameraManagerLayout)==0x28);
    static_assert(offsetof(spCameraManagerLayout,secondaryTable10)==0x10);
    static_assert(offsetof(spCameraManagerLayout,wordsBegin1C)==0x1C);
    inline constexpr std::uint32_t spCameraManagerFactory=0x45B080;
    inline constexpr std::uint32_t spCameraManagerClone=0x45B0E0;
    inline constexpr std::uint32_t spCameraManagerDelete=0x45AFE0;
    inline constexpr std::uint32_t spCameraManagerDestructor=0x45AEF0;
    inline constexpr std::uint32_t spCameraManagerTable=0x6E718C;
    inline constexpr std::uint32_t spCameraManagerSecondaryTable=0x6E7188;
    inline constexpr std::uint32_t spCameraManagerSingleton=0x75DB98;
}
