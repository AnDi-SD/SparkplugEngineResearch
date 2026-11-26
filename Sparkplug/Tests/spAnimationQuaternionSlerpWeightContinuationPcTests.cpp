#include "Code/Sparkplug/spAnimationQuaternionSlerpWeightContinuationPc.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using namespace sparkplug::reconstruction;
    using Sine = AnimationSlerpSine80ForAnalysis;

    bool ReadSine(std::string_view raw, Sine& sine)
    {
        if (raw.size() != 20)
            return false;
        sine = {};
        for (std::size_t i = 0; i < 10; ++i)
        {
            const auto* begin = raw.data() + 2 * i;
            unsigned part = 0;
            const auto parsed = std::from_chars(begin, begin + 2, part, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
                return false;
            if (i < 8)
                sine.significand |= std::uint64_t(part) << (8 * i);
            else
                sine.signExponent |= static_cast<std::uint16_t>(part << (8 * (i - 8)));
        }
        return true;
    }

    void PrintWeight(AnimationSlerpBlendWeight80ForAnalysis weight)
    {
        std::cout << std::hex << std::setfill('0');
        for (unsigned i = 0; i < 8; ++i)
            std::cout << std::setw(2) << ((weight.significand >> (8 * i)) & 0xFFu);
        for (unsigned i = 0; i < 2; ++i)
            std::cout << std::setw(2) << ((weight.signExponent >> (8 * i)) & 0xFFu);
    }

    int Replay()
    {
        std::string row;
        while (std::getline(std::cin, row))
        {
            std::istringstream input(row);
            std::string physical;
            char phase = 0;
            Sine sine{};
            std::uint32_t reciprocal = 0;
            if (!(input >> phase >> physical >> std::hex >> reciprocal) || !ReadSine(physical, sine))
                return 1;

            if (phase == 'A')
            {
                std::uint32_t theta = 0, factor = 0;
                AnimationSlerpPostSineAPcForAnalysis output{};
                if (!(input >> theta >> factor) || !(input >> std::ws).eof() ||
                    !TryFinishAnimationQuaternionSlerpFirstWeightPcForAnalysis(sine, reciprocal, theta, factor, output))
                    return 1;
                std::cout << "A " << std::hex << std::setfill('0') << std::setw(8) << output.weightABits
                          << ' ' << std::setw(8) << output.angleBBits << '\n';
            }
            else if (phase == 'B')
            {
                AnimationSlerpBlendWeight80ForAnalysis output{};
                if (!(input >> std::ws).eof() ||
                    !TryFinishAnimationQuaternionSlerpSecondWeightPcForAnalysis(sine, reciprocal, output))
                    return 1;
                std::cout << "B ";
                PrintWeight(output);
                std::cout << '\n';
            }
            else
                return 1;
        }
        return std::cin.eof() && std::cout ? 0 : 1;
    }

    bool CheckA(Sine sine, std::uint32_t reciprocal, std::uint32_t theta, std::uint32_t factor,
                std::uint32_t weightA, std::uint32_t angleB)
    {
        AnimationSlerpPostSineAPcForAnalysis actual{};
        return TryFinishAnimationQuaternionSlerpFirstWeightPcForAnalysis(sine, reciprocal, theta, factor, actual) &&
               actual.weightABits == weightA && actual.angleBBits == angleB;
    }

    bool CheckB(Sine sine, std::uint32_t reciprocal, AnimationSlerpBlendWeight80ForAnalysis expected)
    {
        AnimationSlerpBlendWeight80ForAnalysis actual{};
        return TryFinishAnimationQuaternionSlerpSecondWeightPcForAnalysis(sine, reciprocal, actual) &&
               actual.significand == expected.significand && actual.signExponent == expected.signExponent;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 2;

    constexpr std::uint32_t One = 0x3F800000u;
    constexpr Sine Midpoint{UINT64_C(0x8000008000000000), 0x3FFEu};
    constexpr Sine Above{Midpoint.significand + 1, Midpoint.signExponent};
    constexpr Sine NegZero{0, 0x8000u};
    bool passed = CheckA(Above, One, One, 0x3F000000u, 0x3F000001u, 0x3F000000u);
    passed &= CheckA(Midpoint, One, One, 0x3F000000u, 0x3F000000u, 0x3F000000u);
    passed &= CheckA(NegZero, One, One, 0x80000000u, 0x80000000u, 0x80000000u);
    passed &= CheckA(Midpoint, One, 0x1F800000u, 0x1F800000u, 0x3F000000u, 0x00200000u);
    passed &= CheckB(Above, One, {Above.significand, Above.signExponent});
    passed &= CheckB(NegZero, One, {0, 0x8000u});
    AnimationSlerpPostSineAPcForAnalysis protectedA{11, 12};
    passed &= !TryFinishAnimationQuaternionSlerpFirstWeightPcForAnalysis(Above, 0, One, One, protectedA) &&
              protectedA.weightABits == 11 && protectedA.angleBBits == 12;
    AnimationSlerpBlendWeight80ForAnalysis protectedB{13, 14};
    passed &= !TryFinishAnimationQuaternionSlerpSecondWeightPcForAnalysis({1, 0}, One, protectedB) &&
              protectedB.significand == 13 && protectedB.signExponent == 14;
    if (!passed)
    {
        std::cerr << "Slerp post-sine arithmetic contract failed\n";
        return 1;
    }
    std::cout << "AnimationQuaternionSlerpWeightContinuationPc: 8 contracts passed\n";
    return 0;
}
