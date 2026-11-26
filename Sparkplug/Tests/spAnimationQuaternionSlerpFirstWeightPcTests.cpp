#include "Code/Sparkplug/spAnimationQuaternionSlerpFirstWeightPc.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using namespace sparkplug::reconstruction;
    using Result = AnimationSlerpFirstWeightPcForAnalysis;

    constexpr std::uint32_t One = 0x3F800000u;
    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr std::uint32_t PiOverTwo = 0x3FC90FDBu;
    constexpr std::uint32_t OneThird = 0x3EAAAAABu;
    constexpr AnimationSlerpSine80ForAnalysis RoundedUp{0xFFFFFF000000FFFEull, 0x3FFE};
    constexpr AnimationSlerpSine80ForAnalysis Tie{0xFFFFFF000000FFFFull, 0x3FFE};
    constexpr AnimationSlerpSine80ForAnalysis Threshold{0x83126F0000000000ull, 0x3FF5};
    constexpr AnimationSlerpSine80ForAnalysis UnitSine{0x8000000000000000ull, 0x3FFF};

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            AnimationSlerpSine80ForAnalysis sine{};
            std::uint32_t theta = 0, factor = 0;
            if (!(fields >> std::hex >> sine.significand >> sine.signExponent >> theta >> factor))
                return 1;
            Result actual{};
            std::string extra;
            if (fields >> extra || !TryPrepareAnimationQuaternionSlerpFirstWeightPcForAnalysis(sine, theta, factor, actual))
                return 1;
            std::cout << std::hex << std::setfill('0') << std::setw(8) << actual.inverseSineBits
                      << ' ' << std::setw(8) << actual.angleBits << '\n';
        }
        return std::cin.eof() && std::cout ? 0 : 1;
    }

    bool Check(AnimationSlerpSine80ForAnalysis sine, std::uint32_t theta, std::uint32_t factor, Result expected)
    {
        Result actual{};
        return TryPrepareAnimationQuaternionSlerpFirstWeightPcForAnalysis(sine, theta, factor, actual) &&
            actual.inverseSineBits == expected.inverseSineBits && actual.angleBits == expected.angleBits;
    }

    bool Reject(AnimationSlerpSine80ForAnalysis sine, std::uint32_t theta, std::uint32_t factor)
    {
        Result actual{1, 2};
        return !TryPrepareAnimationQuaternionSlerpFirstWeightPcForAnalysis(sine, theta, factor, actual) &&
            actual.inverseSineBits == 1 && actual.angleBits == 2;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1)
        return argc == 2 && std::string_view(argv[1]) == "--replay" ? Replay() : 1;

    const std::array cases{
        Check(RoundedUp, PiOverTwo, OneThird, {One + 1, 0x3F860A92u}),
        Check(Tie, PiOverTwo, OneThird, {One, 0x3F860A92u}),
        Check(Threshold, 0x3F000000u, NegativeZero, {0x4479FFFFu, 0x3F000000u}),
        Check(UnitSine, NegativeZero, One, {One, NegativeZero}),
        Reject({Threshold.significand - 1, Threshold.signExponent}, PiOverTwo, OneThird),
        Reject({UnitSine.significand + 1, UnitSine.signExponent}, PiOverTwo, OneThird),
        Reject(UnitSine, PiOverTwo, One + 1),
    };
    for (std::size_t i = 0; i < cases.size(); ++i)
    {
        if (!cases[i])
        {
            std::cerr << "Slerp first-weight contract " << i + 1 << " failed\n";
            return 1;
        }
    }
    std::cout << "AnimationQuaternionSlerpFirstWeightPc: " << cases.size() << " contracts passed\n";
    return 0;
}
