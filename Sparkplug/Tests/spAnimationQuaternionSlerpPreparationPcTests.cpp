#include "Code/Sparkplug/spAnimationQuaternionSlerpPreparationPc.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::AnimationQuaternionPcWordsForAnalysis;
    using sparkplug::reconstruction::AnimationQuaternionSlerpPreparationPcForAnalysis;
    using sparkplug::reconstruction::TryPrepareAnimationQuaternionSlerpPcForAnalysis;
    using Words = AnimationQuaternionPcWordsForAnalysis;
    using Result = AnimationQuaternionSlerpPreparationPcForAnalysis;

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            Words first{};
            Words second{};
            input >> std::hex;
            for (auto& word : first)
                input >> word;
            for (auto& word : second)
                input >> word;
            if (!input || !(input >> std::ws).eof())
                return 1;

            Result result{};
            if (!TryPrepareAnimationQuaternionSlerpPcForAnalysis(first, second, result))
                return 1;
            std::cout << std::hex << std::setfill('0') << std::setw(8) << result.dotBits;
            for (auto word : result.second)
                std::cout << ' ' << std::setw(8) << word;
            std::cout << '\n';
        }
        return std::cin.eof() ? 0 : 1;
    }

    bool Check(std::string_view label, bool passed)
    {
        if (!passed)
            std::cerr << label << " failed\n";
        return passed;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 2;

    constexpr std::uint32_t One = 0x3F800000u;
    constexpr std::uint32_t NegativeOne = 0xBF800000u;
    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr std::uint32_t Small = 0x30800000u;
    constexpr std::uint32_t NegativeSmall = 0xB0800000u;
    constexpr Words Axis{One, 0, 0, 0};
    constexpr Words Mixed{0x3F000000u, NegativeZero, 0x3F400000u, 0};

    Result output{};
    bool passed = Check("positive dot", TryPrepareAnimationQuaternionSlerpPcForAnalysis(Axis, Mixed, output) &&
                                         output.dotBits == 0x3F000000u && output.second == Mixed);

    constexpr Words Left{One, Small, One, 0};
    constexpr Words Right{NegativeOne, NegativeSmall, One, NegativeZero};
    constexpr Words Flipped{One, Small, NegativeOne, 0};
    passed &= Check("extended cancellation", TryPrepareAnimationQuaternionSlerpPcForAnalysis(Left, Right, output) &&
                                              output.dotBits == 0x21800000u && output.second == Flipped);

    constexpr Words Zeros{};
    constexpr Words NegativeWords{NegativeOne, NegativeOne, NegativeOne, NegativeOne};
    passed &= Check("negative zero", TryPrepareAnimationQuaternionSlerpPcForAnalysis(Zeros, NegativeWords, output) &&
                                     output.dotBits == NegativeZero && output.second == NegativeWords);

    constexpr Words Wide{0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u};
    constexpr Words ClampInput{NegativeOne, NegativeZero, 0, NegativeZero};
    constexpr Words ClampOutput{One, 0, NegativeZero, 0};
    passed &= Check("flip then clamp", TryPrepareAnimationQuaternionSlerpPcForAnalysis(Wide, ClampInput, output) &&
                                       output.dotBits == One && output.second == ClampOutput);

    auto invalid = Axis;
    invalid[1] = 0x7FC10203u;
    passed &= Check("refusal preserves result", !TryPrepareAnimationQuaternionSlerpPcForAnalysis(Axis, invalid, output) &&
                                                  output.dotBits == One && output.second == ClampOutput);

    if (!passed)
        return 1;
    std::cout << "AnimationQuaternionSlerpPreparationPc: 5 contracts passed\n";
    return 0;
}
