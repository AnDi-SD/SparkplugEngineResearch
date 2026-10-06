#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc::template_class
{
    inline constexpr std::uint32_t ClassID = 0x6D86570A;
    inline constexpr std::uint32_t Constructor = 0x0059F540;
    inline constexpr std::uint32_t Factory = 0x0059F630;
    inline constexpr std::uint32_t Clone = 0x0059F690;
    inline constexpr std::uint32_t Copy = 0x0059F1B0;
    inline constexpr std::uint32_t DependencyCheck = 0x0059EBD0;
    inline constexpr std::uint32_t CountDescriptors = 0x0059E910;
    inline constexpr std::uint32_t CreateInstance = 0x0059E7A0;
    inline constexpr std::uint32_t ReadWithSerializer = 0x0059E930;
    inline constexpr std::uint32_t ResolveParentNode = 0x0059E990;
    inline constexpr std::uint32_t SetRuntimeState = 0x0059FBA0;
    inline constexpr std::uint32_t ClearDescriptors = 0x0059F130;
    inline constexpr std::uint32_t Destructor = 0x005A0490;
    inline constexpr std::uint32_t PrimaryTable = 0x00703D24;

    // Raw 32-bit evidence only; this is not the portable C++ representation.
    struct Layout final
    {
        std::array<std::byte, 0x14> namedPrefix;
        std::uint32_t descriptorHead14, descriptorTail18, descriptorCount1C;
        std::uint32_t opaqueWord20, opaqueWord24, runtimeRoot28;
        std::array<std::byte, 0x0C> bufferList2C;
        std::array<std::byte, 0x1C> msvcString38;
        std::uint8_t runtimeByte54;
        std::array<std::byte, 3> padding55;
        std::uint32_t opaqueWord58, opaqueWord5C;
    };
    static_assert(sizeof(Layout) == 0x60);
    static_assert(offsetof(Layout, descriptorHead14) == 0x14);
    static_assert(offsetof(Layout, opaqueWord20) == 0x20);
    static_assert(offsetof(Layout, runtimeRoot28) == 0x28);
    static_assert(offsetof(Layout, bufferList2C) == 0x2C);
    static_assert(offsetof(Layout, msvcString38) == 0x38);
    static_assert(offsetof(Layout, runtimeByte54) == 0x54);
}

