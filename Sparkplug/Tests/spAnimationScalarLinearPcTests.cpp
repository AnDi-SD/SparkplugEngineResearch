#include "Code/Sparkplug/spAnimationScalarLinearPc.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    using sparkplug::reconstruction::TrySampleAnimationScalarLinearPcForAnalysis;
    using Word = std::uint32_t;

    enum class Alias { Separate, Left, Right };

    struct SampleCase
    {
        std::size_t leftIndex;
        std::size_t rightIndex;
        Alias alias;
        Word factor;
        Word left;
        Word right;
        Word expected;
    };

    constexpr Word Sentinel = 0xA5A5A5A5u;

    bool SelectAlias(const std::string& token, Alias& alias)
    {
        if (token == "separate")
            alias = Alias::Separate;
        else if (token == "left")
            alias = Alias::Left;
        else if (token == "right")
            alias = Alias::Right;
        else
            return false;
        return true;
    }

    bool ReadWord(std::istream& input, Word& result)
    {
        std::string token;
        if (!(input >> token) || token.size() != 8)
            return false;
        const auto converted = std::from_chars(token.data(), token.data() + token.size(), result, 16);
        return converted.ec == std::errc{} && converted.ptr == token.data() + token.size();
    }

    bool Evaluate(const SampleCase& sample, Word& result)
    {
        std::array<Word, 4> words{0x10203040u, 0x11223344u, 0x55667788u, 0x99AABBCCu};
        if (sample.leftIndex >= words.size() || sample.rightIndex >= words.size() ||
            (sample.rightIndex != sample.leftIndex && sample.rightIndex != sample.leftIndex + 1) ||
            (sample.leftIndex == sample.rightIndex && sample.left != sample.right))
            return false;

        words[sample.leftIndex] = sample.left;
        words[sample.rightIndex] = sample.right;
        const auto before = words;
        Word separate = Sentinel;
        Word* output = &separate;
        if (sample.alias == Alias::Left)
            output = &words[sample.leftIndex];
        else if (sample.alias == Alias::Right)
            output = &words[sample.rightIndex];

        if (!TrySampleAnimationScalarLinearPcForAnalysis(words[sample.leftIndex], words[sample.rightIndex],
                                                         sample.factor, *output))
            return false;
        result = *output;
        auto expected = before;
        if (sample.alias == Alias::Left)
            expected[sample.leftIndex] = result;
        else if (sample.alias == Alias::Right)
            expected[sample.rightIndex] = result;
        return words == expected && (sample.alias == Alias::Separate || separate == Sentinel);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            SampleCase sample{};
            std::string alias, extra;
            Word output;
            if (!(input >> sample.leftIndex >> sample.rightIndex >> alias) || !SelectAlias(alias, sample.alias) ||
                !ReadWord(input, sample.factor) || !ReadWord(input, sample.left) || !ReadWord(input, sample.right) ||
                (input >> extra) || !Evaluate(sample, output))
                return 1;
            std::cout << std::hex << std::setfill('0') << std::setw(8) << output << '\n';
        }
        return std::cin.eof() ? 0 : 1;
    }
}

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 1;

    const std::array samples{
        SampleCase{0, 1, Alias::Separate, 0x3E800000u, 0x3F800000u, 0x41100000u, 0x40400000u},
        SampleCase{2, 3, Alias::Separate, 0x2F800000u, 0x2F800001u, 0xBF800001u, 0x9F800001u},
        SampleCase{0, 1, Alias::Left, 0x3F000000u, 0x80000000u, 0x80000000u, 0x80000000u},
        SampleCase{1, 2, Alias::Right, 0x3F000000u, 0x80000000u, 0, 0},
        SampleCase{0, 1, Alias::Separate, 0x80000000u, 0x80000000u, 0x3F800000u, 0x80000000u},
        SampleCase{3, 3, Alias::Left, 0x3F800000u, 0x3F800001u, 0x3F800001u, 0x3F800001u},
        SampleCase{2, 2, Alias::Right, 0x3EAAAAABu, 0xC5800000u, 0xC5800000u, 0xC5800000u},
        SampleCase{0, 1, Alias::Right, 0x3F000000u, 0x3F800000u, 0x3F800001u, 0x3F800000u},
        SampleCase{2, 3, Alias::Separate, 0x3F000000u, 0x3F800001u, 0x3F800002u, 0x3F800002u},
        SampleCase{0, 1, Alias::Left, 0x3F000000u, 0xBF800000u, 0x3F800000u, 0},
    };
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        Word output;
        if (!Evaluate(samples[i], output) || output != samples[i].expected)
        {
            std::cerr << "Unexpected scalar linear output at case " << i << '\n';
            return 1;
        }
    }

    const std::array rejected{
        std::array<Word, 3>{0x7FC00000u, 0, 0x3F000000u},
        std::array<Word, 3>{0, 0x00000001u, 0x3F000000u},
        std::array<Word, 3>{0, 0, 0xBF000000u},
        std::array<Word, 3>{0, 0, 0x3F800001u},
        std::array<Word, 3>{0, 0, 0x2F7FFFFFu},
    };
    for (const auto& [left, right, factor] : rejected)
    {
        Word output = Sentinel;
        if (TrySampleAnimationScalarLinearPcForAnalysis(left, right, factor, output) || output != Sentinel)
            return 1;
    }
    std::cout << "AnimationScalarLinearPc: 10 cases, 5 refusals\n";
    return 0;
}
