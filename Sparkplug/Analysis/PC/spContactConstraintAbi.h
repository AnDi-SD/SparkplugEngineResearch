#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Observed PC byte offsets only. Unknown spans remain opaque; this is not
    // a recovered spConstraint base layout or portable C++ memory overlay.
    struct spContactConstraintObservedLayout final
    {
        std::array<std::uint8_t, 0x58> opaque00;
        std::uint32_t rawWord58;
        std::uint32_t rawWord5C;
        std::array<std::uint8_t, 0x28> opaque60;
        std::uint8_t rawMode88;
        std::array<std::uint8_t, 3> opaque89;
        std::uint32_t rawCoefficient8C;
        std::uint32_t rawCoefficient90;
        std::array<std::uint8_t, 0x50> opaque94;
    };
    static_assert(sizeof(spContactConstraintObservedLayout) == 0xE4);
    static_assert(offsetof(spContactConstraintObservedLayout, rawWord58) == 0x58);
    static_assert(offsetof(spContactConstraintObservedLayout, rawWord5C) == 0x5C);
    static_assert(offsetof(spContactConstraintObservedLayout, rawMode88) == 0x88);
    static_assert(offsetof(spContactConstraintObservedLayout, rawCoefficient8C) == 0x8C);
    static_assert(offsetof(spContactConstraintObservedLayout, rawCoefficient90) == 0x90);

    inline constexpr std::uint32_t spContactConstraintPrimary = 0x6E7090;
    inline constexpr std::uint32_t spContactConstraintFactory = 0x48BCF0;
    inline constexpr std::uint32_t spContactConstraintGetter = 0x458F40;
    inline constexpr std::uint32_t spContactConstraintRecord = 0x7619B0;
    inline constexpr std::uint32_t spContactConstraintOwn1C = 0x48B550;
    inline constexpr std::size_t spContactConstraintOwn1CBytes = 186;
    inline constexpr std::uint32_t spContactConstraintOwn24 = 0x48BCD0;
    inline constexpr std::size_t spContactConstraintOwn24Bytes = 21;
    inline constexpr std::uint32_t spContactConstraintOwn24Helper = 0x48BC70;
    inline constexpr std::size_t spContactConstraintOwn24HelperBytes = 96;
    inline constexpr std::size_t spContactConstraintOwn24SourceBytes = 92;
    inline constexpr std::size_t spContactConstraintOwn24DestinationOffset = 0x88;
    inline constexpr std::size_t spContactConstraintRaw58Offset = 0x58;
    inline constexpr std::size_t spContactConstraintRaw5COffset = 0x5C;
    inline constexpr std::size_t spContactConstraintRaw88Offset = 0x88;
    inline constexpr std::size_t spContactConstraintRaw8COffset = 0x8C;
    inline constexpr std::size_t spContactConstraintRaw90Offset = 0x90;
}
