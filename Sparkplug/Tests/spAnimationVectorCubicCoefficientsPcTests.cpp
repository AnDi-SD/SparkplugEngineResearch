#include "Code/Sparkplug/spAnimationVectorCubicCoefficientsPc.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace sparkplug::reconstruction;
    using Record = AnimationVectorCubicPcRecordForAnalysis;
    using Words = AnimationVectorCubicPcWordsForAnalysis;

    constexpr std::uint32_t Poison = 0x7FC12345u;
    constexpr std::uint32_t Tiny = 0x21800000u;
    constexpr std::uint32_t One = 0x3F800000u;

    bool Prepare(Record* keys, std::size_t count)
    {
        return TryPrepareAnimationVectorCubicCoefficientsPcForAnalysis(keys, count);
    }

    Words Fill(std::uint32_t value)
    {
        return {value, value, value};
    }

    std::array<Record, 2> Pair(std::uint32_t first, std::uint32_t second,
                               std::uint32_t outgoing, std::uint32_t incoming)
    {
        const auto unused = Fill(Poison);
        return {{{Fill(first), unused, Fill(outgoing), unused, unused},
                 {Fill(second), Fill(incoming), unused, unused, unused}}};
    }

    bool Same(const Record& left, const Record& right)
    {
        return left.value == right.value && left.incomingTangent == right.incomingTangent &&
            left.outgoingTangent == right.outgoingTangent && left.quadratic == right.quadratic &&
            left.cubic == right.cubic;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            std::size_t count;
            if (!(input >> count) || count < 1 || count > 1024)
                return 1;
            std::vector<Record> records(count);
            input >> std::hex;
            for (auto& key : records)
            {
                for (auto* words : {&key.value, &key.incomingTangent, &key.outgoingTangent, &key.quadratic, &key.cubic})
                {
                    for (auto& word : *words)
                    {
                        if (!(input >> word))
                            return 1;
                    }
                }
            }
            std::string extra;
            if (input >> extra || !Prepare(records.data(), records.size()))
                return 1;

            std::cout << std::dec << count << std::hex << std::setfill('0');
            for (const auto& key : records)
            {
                for (const auto* words : {&key.value, &key.incomingTangent, &key.outgoingTangent, &key.quadratic, &key.cubic})
                {
                    for (const auto word : *words)
                        std::cout << ' ' << std::setw(8) << word;
                }
            }
            std::cout << '\n';
        }
        return std::cin.eof() && std::cout ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1)
        return argc == 2 && std::string_view(argv[1]) == "--replay" ? Replay() : 1;

    auto midpoint = Pair(Tiny, One + 1, 0, 0);
    auto cubic = Pair(0, One, 0x40000000u, Tiny);
    auto tangent = Pair(0, One, 0x3FC00000u, Tiny);
    auto single = Pair(Poison, One, Poison, 0);
    auto refused = Pair(0, One, 0, Poison);
    auto multiple = Pair(One, One, 0, 0);
    std::array<Record, 3> laterRefused{multiple[0], multiple[1], refused[1]};
    laterRefused[1].outgoingTangent = Fill(0);
    const auto before = laterRefused;

    const std::array cases{
        Prepare(midpoint.data(), midpoint.size()) && midpoint[0].quadratic == Words{0x40400001u, 0x40400001u, 0x40400002u},
        Prepare(cubic.data(), cubic.size()) && cubic[0].cubic == Words{Tiny, 0, 0},
        Prepare(tangent.data(), tangent.size()) && tangent[0].quadratic == Words{0, 0, 0xA1800000u},
        Prepare(single.data(), 1) && Same(single[0], Pair(Poison, One, Poison, 0)[0]),
        !Prepare(nullptr, 2),
        !Prepare(refused.data(), 0),
        !Prepare(refused.data(), refused.size()) && Same(refused[0], Pair(0, One, 0, Poison)[0]),
        !Prepare(laterRefused.data(), laterRefused.size()) && Same(laterRefused[0], before[0]) &&
            Same(laterRefused[1], before[1]) && Same(laterRefused[2], before[2]),
        Same(midpoint[1], Pair(Tiny, One + 1, 0, 0)[1]),
    };
    for (std::size_t i = 0; i < cases.size(); ++i)
    {
        if (!cases[i])
        {
            std::cerr << "Packed XYZ cubic coefficient case " << i + 1 << " failed\n";
            return 1;
        }
    }
    std::cout << cases.size() << " packed XYZ cubic coefficient checks passed\n";
    return 0;
}
