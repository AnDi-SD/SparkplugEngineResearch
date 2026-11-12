#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Native function argument views only; no native class or portable ABI.
    struct ContactTangentArguments final
    {
        std::uint32_t inputAddress;
        std::uint32_t firstOutputAddress;
        std::uint32_t secondOutputAddress;
    };
    struct ContactTangentVector final { std::uint32_t words[3]; };
    static_assert(sizeof(ContactTangentArguments) == 12);
    static_assert(sizeof(ContactTangentVector) == 12);
    inline constexpr std::uint32_t ContactTangentHelper = 0x48B220;
    inline constexpr std::size_t ContactTangentHelperBytes = 183;
    inline constexpr std::uint32_t ContactTangentReturns[2]{0x48B28A, 0x48B2D6};
    inline constexpr std::uint32_t ContactTangentCaller = 0x48B990;
    inline constexpr std::uint32_t ContactTangentThresholdAddress = 0x6EC378;
    inline constexpr std::uint32_t ContactTangentThresholdBits = 0x3F3504F3;
}
