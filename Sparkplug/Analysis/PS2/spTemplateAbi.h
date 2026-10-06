#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2::template_class
{
    inline constexpr std::uint32_t ClassID = 0x6D86570A;
    inline constexpr std::uint32_t Constructor = 0x00153C10;
    inline constexpr std::uint32_t Factory = 0x00153D70;
    inline constexpr std::uint32_t Clone = 0x00153CB0;
    inline constexpr std::uint32_t Copy = 0x00153680;
    inline constexpr std::uint32_t DependencyCheck = 0x00153880;
    inline constexpr std::uint32_t CountDescriptors = 0x00153920;
    inline constexpr std::uint32_t CreateInstance = 0x00153570;
    inline constexpr std::uint32_t ReadWithSerializer = 0x001535C0;
    inline constexpr std::uint32_t ResolveParentNode = 0x00153450;
    inline constexpr std::uint32_t SetRuntimeState = 0x00152920;
    inline constexpr std::uint32_t Destructor = 0x00153950;
    inline constexpr std::uint32_t PrimaryTable = 0x0048E070;

    struct Layout final
    {
        std::array<std::byte, 0x14> namedPrefix;
        std::uint32_t descriptorHead14, descriptorTail18, descriptorCount1C;
        std::uint32_t opaqueWord20, opaqueWord24, runtimeRoot28;
        std::array<std::byte, 0x0C> bufferList2C;
        std::array<std::byte, 0x0C> string38;
        std::uint8_t runtimeByte44;
        std::array<std::byte, 3> padding45;
        std::uint32_t opaqueWord48, opaqueWord4C;
    };
    static_assert(sizeof(Layout) == 0x50);
    static_assert(offsetof(Layout, descriptorHead14) == 0x14);
    static_assert(offsetof(Layout, opaqueWord20) == 0x20);
    static_assert(offsetof(Layout, runtimeRoot28) == 0x28);
    static_assert(offsetof(Layout, bufferList2C) == 0x2C);
    static_assert(offsetof(Layout, string38) == 0x38);
    static_assert(offsetof(Layout, runtimeByte44) == 0x44);
}

