#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Function argument/output views only; no native class or source C++ ABI.
    struct SphereContactArguments final
    {
        std::uint32_t center1Address;
        std::uint32_t radius1Bits;
        std::uint32_t center2Address;
        std::uint32_t radius2Bits;
        std::uint32_t outputAddress;
    };
    struct SphereContactOutput final
    {
        std::uint32_t positionBits[3];
        std::uint32_t normalBits[3];
        std::uint32_t depthBits;
    };
    static_assert(sizeof(SphereContactArguments) == 20);
    static_assert(sizeof(SphereContactOutput) == 28);
    static_assert(offsetof(SphereContactOutput, normalBits) == 0x0C);
    static_assert(offsetof(SphereContactOutput, depthBits) == 0x18);
    inline constexpr std::uint32_t SphereContactHelper = 0x472270;
    inline constexpr std::uint32_t SphereContactHelperBytes = 318;
    inline constexpr std::array<std::uint32_t, 3> SphereContactReturns{0x472305, 0x4723A5, 0x4723AD};
    inline constexpr std::uint32_t SpherePairCaller = 0x472500;
    inline constexpr std::uint32_t SpherePairHelperCall = 0x47252E;
    inline constexpr std::uint32_t SpherePairDispatchStore = 0x456B5B;
    inline constexpr std::uint32_t SpherePairDispatchCell = 0x75FA50;
}
