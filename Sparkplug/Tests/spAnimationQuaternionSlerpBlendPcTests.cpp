#include "Code/Sparkplug/spAnimationQuaternionSlerpBlendPc.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using namespace sparkplug::reconstruction;
    using Quaternion = AnimationQuaternionPcWordsForAnalysis;
    using Weight = AnimationSlerpBlendWeight80ForAnalysis;

    constexpr std::uint32_t One = 0x3F800000u;
    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr std::uint32_t Small = 0x2F800000u;
    constexpr Quaternion Ones{One, One, One, One};
    constexpr Quaternion SmallWords{Small, Small, Small, Small};
    constexpr Weight RetainedZ{0x8000000008000000ull, 0x3FE7};
    constexpr Weight RetainedW{0x8004000000000000ull, 0x3FD0};
    constexpr Weight SubnormalZ{0xFFFFFFFFFFFFFFFEull, 0xBFDE};
    constexpr Quaternion Sentinel{1, 2, 3, 4};

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            unsigned alias = 0;
            std::uint32_t alpha = 0;
            Weight beta{};
            Quaternion first{}, second{}, separate{};
            if (!(fields >> alias >> std::hex >> alpha >> beta.significand >> beta.signExponent) || alias > 2)
                return 1;
            for (auto* endpoint : {&first, &second})
            {
                for (auto& word : *endpoint)
                {
                    if (!(fields >> word))
                        return 1;
                }
            }
            std::string extra;
            std::array destinations{&separate, &first, &second};
            auto& destination = *destinations[alias];
            if (fields >> extra || !TryBlendAnimationQuaternionSlerpPcForAnalysis(first, second, alpha, beta, destination))
                return 1;
            std::cout << std::hex << std::setfill('0') << std::setw(8) << destination[0];
            for (std::size_t axis = 1; axis < destination.size(); ++axis)
                std::cout << ' ' << std::setw(8) << destination[axis];
            std::cout << '\n';
        }
        return std::cin.eof() && std::cout ? 0 : 1;
    }

    bool Check(Quaternion first, Quaternion second, std::uint32_t alpha, Weight beta,
               Quaternion expected, unsigned alias = 0)
    {
        Quaternion separate = Sentinel;
        std::array destinations{&separate, &first, &second};
        auto& destination = *destinations[alias];
        return TryBlendAnimationQuaternionSlerpPcForAnalysis(first, second, alpha, beta, destination) && destination == expected;
    }

    bool Reject(std::uint32_t alpha, Weight beta)
    {
        auto actual = Sentinel;
        return !TryBlendAnimationQuaternionSlerpPcForAnalysis(Ones, Ones, alpha, beta, actual) && actual == Sentinel;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1)
        return argc == 2 && std::string_view(argv[1]) == "--replay" ? Replay() : 1;

    constexpr Quaternion BelowOne{One - 1, One - 1, One - 1, One - 1};
    constexpr Quaternion ZeroSigns{NegativeZero, 0, NegativeZero, 0};
    const std::array cases{
        Check(Ones, Ones, One, RetainedZ, {One, One, One + 1, One}),
        Check(BelowOne, Ones, One + 1, RetainedW, {One, One, One, One + 1}, 2),
        Check(SmallWords, SmallWords, Small, SubnormalZ, {0, 0, 0x00400000u, 0}),
        Check(ZeroSigns, {One, One | NegativeZero, One, One | NegativeZero}, One, {0, 0x8000}, ZeroSigns, 1),
        Reject(0x7FC00000u, RetainedZ),
        Reject(One, {0xC000000000000000ull, 0x7FFF}),
        Reject(One, {1, 0x3FFF}),
    };
    for (std::size_t i = 0; i < cases.size(); ++i)
    {
        if (!cases[i])
        {
            std::cerr << "Slerp blend contract " << i + 1 << " failed\n";
            return 1;
        }
    }
    std::cout << "AnimationQuaternionSlerpBlendPc: " << cases.size() << " contracts passed\n";
    return 0;
}
