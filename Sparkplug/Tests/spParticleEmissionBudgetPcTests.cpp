#include "Code/Sparkplug/spParticleEmissionBudgetPc.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::ParticleEmissionBudgetPcForAnalysis;
    using sparkplug::reconstruction::ParticleEmissionBudgetPcInputForAnalysis;
    using sparkplug::reconstruction::TryEvaluateParticleEmissionBudgetPcNearestForAnalysis;
    using Words = std::array<std::uint32_t, 7>;
    using Projection = std::array<std::uint32_t, 5>;

    struct Witness
    {
        Words input;
        Projection expected;
    };

    constexpr std::array<Witness, 11> Witnesses{{
        // disabled
        {{0x3E000000u, 0x3F000000u, 0x40000000u, 0x40000000u,
          0xBF800000u, 0x40800000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0x3E000000u, 0x3E000000u, 0x3E000000u}},
        // duration-expired
        {{0x3E000000u, 0x3F000000u, 0x3FA00000u, 0x40000000u,
          0x3F800000u, 0x40800000u, 0x00000001u},
         {0x00000000u, 0x00000000u, 0x3E000000u, 0x3E000000u, 0x3E000000u}},
        // duration-equal
        {{0x3E000000u, 0x3EC00000u, 0x3F800000u, 0x40000000u,
          0x3F800000u, 0x40800000u, 0x00000001u},
         {0x00000001u, 0x00000002u, 0x3F000000u, 0x3F000000u, 0x00000000u}},
        // nearest64-boundary
        {{0xA1800000u, 0x3F800000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x3F800000u, 0x00000001u},
         {0x00000001u, 0x00000000u, 0x3F800000u, 0x3F800000u, 0x3F800000u}},
        // cap-taken
        {{0x3FC00000u, 0x3F400000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x40800000u, 0x00000001u},
         {0x00000001u, 0x00000008u, 0x40100000u, 0x40000000u, 0x00000000u}},
        // cap-equal
        {{0x3FC00000u, 0x3F000000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x3FC00000u, 0x00000001u},
         {0x00000001u, 0x00000003u, 0x40000000u, 0x40000000u, 0x00000000u}},
        // zero-rate
        {{0x3E800000u, 0x3D800000u, 0x40400000u, 0x40000000u,
          0xBF800000u, 0x00000000u, 0x00000001u},
         {0x00000001u, 0x00000000u, 0x3EA00000u, 0x3EA00000u, 0x3EA00000u}},
        // negative-ftol-correction
        {{0xBF600000u, 0x00000000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x3F800000u, 0x00000080u},
         {0x00000001u, 0x00000000u, 0xBF600000u, 0xBF600000u, 0xBF600000u}},
        // small-quota-rounded-qword-zero
        {{0x3D800000u, 0x3D000000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x3F800000u, 0x00000001u},
         {0x00000001u, 0x00000000u, 0x3DC00000u, 0x3DC00000u, 0x3DC00000u}},
        // retained-unrounded-request
        {{0x3DCCCCCDu, 0x3F333333u, 0x3FD9999Au, 0x40000000u,
          0xBF800000u, 0x40A00000u, 0x00000001u},
         {0x00000001u, 0x00000003u, 0x3F4CCCCDu, 0x3F4CCCCDu, 0x3E4CCCCEu}},
        // fractional-quotient
        {{0x3F000000u, 0x3E000000u, 0x3F800000u, 0x40000000u,
          0xBF800000u, 0x40400000u, 0x00000001u},
         {0x00000001u, 0x00000001u, 0x3F200000u, 0x3F200000u, 0x3E955555u}},
    }};

    constexpr ParticleEmissionBudgetPcForAnalysis Dirty{true, 0xDEADBEEFu, 0xACCE5510u, 0x00332211u, 0x81001234u};

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    ParticleEmissionBudgetPcInputForAnalysis MakeInput(const Words& words) noexcept
    {
        return {words[0], words[1], words[2], words[3], words[4], words[5], static_cast<std::uint8_t>(words[6])};
    }

    Words InputWords(const ParticleEmissionBudgetPcInputForAnalysis& input) noexcept
    {
        return {input.accumulatorBits, input.deltaBits, input.timeBits, input.lifetimeCeilingBits,
                input.durationBits, input.rateBits, input.enabled};
    }

    Projection Project(const ParticleEmissionBudgetPcForAnalysis& result) noexcept
    {
        return {result.reachesProducer, result.requested, result.provisionalAccumulatorBits,
                result.accumulatorAtProducerBits, result.accumulatorAfterDebitBits};
    }

    Projection Evaluate(const Words& words)
    {
        const auto input = MakeInput(words);
        auto output = Dirty;
        Require(TryEvaluateParticleEmissionBudgetPcNearestForAnalysis(input, output), "qualified particle budget refused");
        Require(InputWords(input) == words, "particle budget modified its captured inputs");
        return Project(output);
    }

    Words Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        Words words{};
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            std::string word;
            const auto width = i + 1 == words.size() ? 2u : 8u;
            if (!(fields >> word) || word.size() != width || word.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("expected six raw binary32 words followed by one gate byte");
            const auto end = word.data() + word.size();
            const auto parsed = std::from_chars(word.data(), end, words[i], 16);
            if (parsed.ec != std::errc{} || parsed.ptr != end)
                throw std::runtime_error("invalid particle budget word");
        }
        std::string extra;
        if (fields >> extra)
            throw std::runtime_error("unexpected trailing replay field");
        return words;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            const auto output = Evaluate(Parse(line));
            std::cout << std::hex << std::setfill('0') << std::setw(2) << output[0];
            for (std::size_t i = 1; i < output.size(); ++i)
                std::cout << ' ' << std::setw(8) << output[i];
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    void CheckRefusal(const Words& words)
    {
        const auto input = MakeInput(words);
        auto output = Dirty;
        Require(!TryEvaluateParticleEmissionBudgetPcNearestForAnalysis(input, output)
                && Project(output) == Project(Dirty) && InputWords(input) == words,
            "unqualified particle budget changed its input or output");
    }

    int RunTests()
    {
        for (const auto& fixture : Witnesses)
            Require(Evaluate(fixture.input) == fixture.expected, "PC particle budget differs");

        struct UnqualifiedWord
        {
            std::size_t offset;
            std::uint32_t bits;
        };
        constexpr UnqualifiedWord outside[]{
            {0, 0x80000000u}, {0, 0x00000001u}, {1, 0xBF800000u}, {2, 0xBF800000u}, {3, 0x00000000u},
            {3, 0xBF800000u}, {4, 0x7F800000u}, {5, 0xFFC12345u}, {5, 0xBF800000u}, {5, 0x4F000001u},
        };
        for (const auto& word : outside)
        {
            auto input = Witnesses[2].input;
            input[word.offset] = word.bits;
            CheckRefusal(input);
        }
        CheckRefusal({0x4F000000u, 0, 0, 0x4F000000u, 0xBF800000u, 0x3F800000u, 1});
        CheckRefusal({0xBF800000u, 0, 0, 0x40000000u, 0xBF800000u, 0x3F800000u, 1});

        std::cout << "ParticleEmissionBudgetPc: " << checks << " contracts passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunTests();
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
