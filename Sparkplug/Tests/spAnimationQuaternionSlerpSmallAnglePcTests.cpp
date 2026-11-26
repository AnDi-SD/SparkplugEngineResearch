#include "Code/Sparkplug/spAnimationQuaternionSlerpSmallAnglePc.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using namespace sparkplug::reconstruction;
    using Branch = AnimationSlerpSmallAngleBranchForAnalysis;
    using Words = AnimationQuaternionPcWordsForAnalysis;
    using Sine = AnimationSlerpSine80ForAnalysis;

    constexpr Words Sentinel{0x7FC12345u, 0x7FC23456u, 0x7FC34567u, 0x7FC45678u};

    bool ReadSine(std::string_view raw, Sine& sine)
    {
        if (raw.size() != 20)
            return false;
        sine = {};
        for (std::size_t index = 0; index < 10; ++index)
        {
            const auto* start = raw.data() + 2 * index;
            unsigned byte = 0;
            const auto parsed = std::from_chars(start, start + 2, byte, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != start + 2)
                return false;
            if (index < 8)
                sine.significand |= std::uint64_t(byte) << (8 * index);
            else
                sine.signExponent |= static_cast<std::uint16_t>(byte << (8 * (index - 8)));
        }
        return true;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            std::string physical;
            Sine sine{};
            Words first{};
            if (!(input >> physical >> std::hex >> first[0] >> first[1] >> first[2] >> first[3]) ||
                !(input >> std::ws).eof() || !ReadSine(physical, sine))
                return 1;

            auto destination = Sentinel;
            const auto branch = ApplyAnimationQuaternionSlerpSmallAnglePcForAnalysis(sine, first, destination);
            if (branch == Branch::Unsupported)
                return 1;
            std::cout << (branch == Branch::CopyFirst ? '1' : '0') << std::hex << std::setfill('0');
            for (auto word : destination)
                std::cout << ' ' << std::setw(8) << word;
            std::cout << '\n';
        }
        return std::cin.eof() ? 0 : 1;
    }

    bool Check(std::string_view name, Sine sine, const Words& first, Branch branch, const Words& expected)
    {
        auto destination = Sentinel;
        if (ApplyAnimationQuaternionSlerpSmallAnglePcForAnalysis(sine, first, destination) == branch && destination == expected)
            return true;
        std::cerr << name << " failed\n";
        return false;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 2;

    constexpr Words First{0x3F800000u, 0x80000000u, 0xBF000000u, 0};
    constexpr Sine Threshold{UINT64_C(0x83126F0000000000), 0x3FF5u};
    const Sine below{Threshold.significand - 1, Threshold.signExponent};
    const Sine above{Threshold.significand + 1, Threshold.signExponent};
    bool passed = Check("below", below, First, Branch::CopyFirst, First);
    passed &= Check("equal", Threshold, First, Branch::Continue, Sentinel);
    passed &= Check("above", above, First, Branch::Continue, Sentinel);
    passed &= Check("negative zero", {0, 0x8000u}, First, Branch::CopyFirst, First);
    passed &= Check("unsupported sine", {1, 0}, First, Branch::Unsupported, Sentinel);
    auto alias = First;
    passed &= ApplyAnimationQuaternionSlerpSmallAnglePcForAnalysis(below, alias, alias) == Branch::CopyFirst && alias == First;
    if (!passed)
        return 1;

    std::cout << "AnimationQuaternionSlerpSmallAnglePc: 6 contracts passed\n";
    return 0;
}
