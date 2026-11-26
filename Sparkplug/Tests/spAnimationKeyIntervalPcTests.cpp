#include "Code/Sparkplug/spAnimationKeyIntervalPc.h"

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using sparkplug::reconstruction::AnimationKeyIntervalPcForAnalysis;
    using sparkplug::reconstruction::TryFindAnimationKeyIntervalPcForAnalysis;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    template <std::size_t Count>
    void CheckInterval(const std::array<std::uint32_t, Count>& keys, std::uint32_t time,
                       std::int32_t cache, std::int32_t index, std::uint32_t factor)
    {
        const auto original = keys;
        AnimationKeyIntervalPcForAnalysis result{-1, 0xFFFFFFFFu};
        Check(TryFindAnimationKeyIntervalPcForAnalysis(keys.data(), keys.size(), time, cache, result),
              "Valid animation key interval was rejected");
        Check(result.index == index && result.factorBits == factor, "Animation key interval mismatch");
        Check(keys == original, "Timestamp input changed");
    }

    void CheckIntervals()
    {
        constexpr std::array<std::uint32_t, 1> single{0x3F800000u};
        constexpr std::array<std::uint32_t, 2> endpoints{0, 0x3F800000u};
        constexpr std::array<std::uint32_t, 5> duplicates{0, 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x40400000u};
        constexpr std::array<std::uint32_t, 2> precision{0xBF800000u, 0x33000000u};
        constexpr std::array<std::uint32_t, 2> wide{0xC5800000u, 0};
        CheckInterval(single, 0, 0, 0, 0);
        CheckInterval(single, 0x3F800000u, 0x7FFFFFFF, 0, 0);
        CheckInterval(endpoints, 0x3F800000u, 0, 0, 0);
        CheckInterval(endpoints, 0x3E800000u, 1, 0, 0x3E800000u);
        CheckInterval(endpoints, 0x80000000u, 0, 0, 0x80000000u);
        CheckInterval(duplicates, 0x3F800000u, 0, 3, 0);
        CheckInterval(duplicates, 0x3F800000u, 4, 3, 0);
        CheckInterval(duplicates, 0x40400000u, 4, 3, 0x3F800000u);
        CheckInterval(precision, 0xB3000000u, 0, 0, 0x3F7FFFFFu);
        CheckInterval(precision, 0xB3000000u, 1, 0, 0x3F7FFFFFu);
        CheckInterval(wide, 0x9F800000u, 0, 0, 0x3F800000u);
    }

    void CheckRefusals()
    {
        constexpr std::array<std::uint32_t, 2> sorted{0, 0x3F800000u};
        constexpr std::array<std::uint32_t, 2> reversed{0x3F800000u, 0};
        constexpr std::array<std::uint32_t, 2> subnormal{1, 0x3F800000u};
        AnimationKeyIntervalPcForAnalysis result{-1, 0xFFFFFFFFu};
        Check(!TryFindAnimationKeyIntervalPcForAnalysis(sorted.data(), sorted.size(), 0, -1, result),
              "Negative cache was accepted");
        Check(!TryFindAnimationKeyIntervalPcForAnalysis(sorted.data(), 0, 0, 0, result),
              "Zero key count was accepted");
        Check(!TryFindAnimationKeyIntervalPcForAnalysis(sorted.data(), sorted.size(), 0x7F800000u, 0, result),
              "Nonfinite time was accepted");
        Check(!TryFindAnimationKeyIntervalPcForAnalysis(reversed.data(), reversed.size(), 0, 0, result),
              "Decreasing key times were accepted");
        Check(!TryFindAnimationKeyIntervalPcForAnalysis(subnormal.data(), subnormal.size(), 0, 0, result),
              "Subnormal key time was accepted");
        Check(result.index == -1 && result.factorBits == 0xFFFFFFFFu, "Refused interval changed the result");
    }

    int Replay()
    {
        std::string row;
        while (std::getline(std::cin, row))
        {
            std::istringstream input(row);
            std::size_t count = 0;
            std::int32_t cache = 0;
            std::uint32_t time = 0;
            Check(bool(input >> count >> cache >> std::hex >> time) && count > 0 && count <= 1024,
                  "Invalid interval replay header");
            std::vector<std::uint32_t> keys(count);
            for (auto& key : keys)
                Check(bool(input >> key), "Incomplete interval replay timestamps");
            input >> std::ws;
            Check(input.eof(), "Unexpected interval replay trailing words");

            AnimationKeyIntervalPcForAnalysis result{};
            Check(TryFindAnimationKeyIntervalPcForAnalysis(keys.data(), keys.size(), time, cache, result),
                  "Interval replay input was rejected");
            std::cout << result.index << ' ' << std::hex << std::setw(8) << std::setfill('0')
                      << result.factorBits << std::dec << '\n';
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
        return Replay();
    Check(argc == 1, "Unexpected interval test arguments");

    CheckIntervals();
    CheckRefusals();
    std::cout << "Animation key interval PC checks passed.\n";
    return 0;
}
