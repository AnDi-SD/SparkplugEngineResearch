#include "Code/Sparkplug/spAnimationVectorLinearPc.h"

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
    using sparkplug::reconstruction::AnimationVectorLinearPcWordsForAnalysis;
    using sparkplug::reconstruction::TryInterpolateAnimationVectorLinearPcForAnalysis;
    using Vector = AnimationVectorLinearPcWordsForAnalysis;

    enum class Destination { Separate, First, Second };

    struct Fixture
    {
        Destination destination;
        std::uint32_t factor;
        Vector first;
        Vector second;
        Vector expected;
    };

    constexpr Vector LeftWitness{0x3e1e7305u, 0x3e1e7305u, 0x3e1e7305u};
    constexpr Vector RightWitness{0x3f3ae8f7u, 0x3f3ae8f7u, 0x3f3ae8f7u};
    constexpr Vector WitnessOutput{0x3e70142fu, 0x3e70142eu, 0x3e70142eu};
    constexpr Vector Marker{0xfef10234u, 0xc1caf123u, 0x7fc12345u};
    constexpr std::array<Fixture, 12> Fixtures{{
        {Destination::Separate, 0x3f000000u, {0x3f800000u, 0x40000000u, 0x40400000u},
            {0x41300000u, 0x41b00000u, 0x42040000u}, {0x40c00000u, 0x41400000u, 0x41900000u}},
        {Destination::Separate, 0x3e0ddecau, LeftWitness, RightWitness, WitnessOutput},
        {Destination::Separate, 0x3effffffu, {0x3f800000u, 0x3f800000u, 0x3f800000u},
            {0xbf800000u, 0xbf800000u, 0xbf800000u}, {0x33800000u, 0x33000000u, 0x33000000u}},
        {Destination::Separate, 0x00000000u, {0x80000000u, 0, 0x80000000u},
            {0x80000000u, 0, 0}, {0x80000000u, 0, 0}},
        {Destination::Separate, 0x80000000u, {0x80000000u, 0, 0x80000000u},
            {0x80000000u, 0, 0}, {0, 0, 0x80000000u}},
        {Destination::Separate, 0x3f800000u, {0x80000000u, 0x3f800000u, 0x45800000u},
            {0x80000000u, 0x45800000u, 0xaf800000u}, {0x80000000u, 0x45800000u, 0xaf800000u}},
        {Destination::Separate, 0x2f800000u, {0, 0x80000000u, 0x3f800000u},
            {0x45800000u, 0xc5800000u, 0x3f800000u}, {0x35800000u, 0xb5800000u, 0x3f800000u}},
        {Destination::Separate, 0x3f7fffffu, {0x45800000u, 0xc5800000u, 0x2f800000u},
            {0xc5800000u, 0x45800000u, 0xaf800000u}, {0xc57ffffeu, 0x457ffffeu, 0xaf7ffffeu}},
        {Destination::First, 0x3e0ddecau, LeftWitness, RightWitness, WitnessOutput},
        {Destination::Second, 0x3e0ddecau, LeftWitness, RightWitness, WitnessOutput},
        {Destination::Second, 0x3f000000u, {0x3f800000u, 0xbf800000u, 0x80000000u},
            {0xbf800000u, 0x3f800000u, 0x80000000u}, {0, 0, 0x80000000u}},
        {Destination::First, 0x3f000001u, {0x2f800000u, 0xaf800000u, 0x45800000u},
            {0xc5800000u, 0x45800000u, 0x2f800000u}, {0xc5000001u, 0x45000001u, 0x44fffffeu}},
    }};

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    Vector Evaluate(const Fixture& fixture)
    {
        auto first = fixture.first;
        auto second = fixture.second;
        auto separate = Marker;
        auto& output = fixture.destination == Destination::First ? first :
                       fixture.destination == Destination::Second ? second : separate;
        Require(TryInterpolateAnimationVectorLinearPcForAnalysis(first, second, fixture.factor, output),
                "packed animation fixture refused");
        Require((fixture.destination == Destination::First || first == fixture.first)
                    && (fixture.destination == Destination::Second || second == fixture.second)
                    && (fixture.destination == Destination::Separate || separate == Marker),
                "interpolation changed a non-destination vector");
        return output;
    }

    std::uint32_t ReadWord(std::istringstream& fields)
    {
        std::string field;
        if (!(fields >> field) || field.size() != 8 ||
            field.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("expected raw binary32 word");
        std::uint32_t word;
        const auto end = field.data() + field.size();
        const auto parsed = std::from_chars(field.data(), end, word, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid binary32 word");
        return word;
    }

    Fixture Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        std::string destination;
        if (!(fields >> destination))
            throw std::runtime_error("missing destination selection");
        Fixture fixture{};
        if (destination == "first")
            fixture.destination = Destination::First;
        else if (destination == "second")
            fixture.destination = Destination::Second;
        else if (destination != "separate")
            throw std::runtime_error("unknown destination selection");
        fixture.factor = ReadWord(fields);
        for (auto& word : fixture.first)
            word = ReadWord(fields);
        for (auto& word : fixture.second)
            word = ReadWord(fields);
        if (fields >> destination)
            throw std::runtime_error("unexpected animation fixture field");
        return fixture;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            const auto output = Evaluate(Parse(line));
            std::cout << std::hex << std::setfill('0') << std::setw(8) << output[0] << ' '
                      << std::setw(8) << output[1] << ' ' << std::setw(8) << output[2] << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "animation replay stream failed");
        return 0;
    }

    int RunTests()
    {
        for (const auto& fixture : Fixtures)
            Require(Evaluate(fixture) == fixture.expected, "packed animation raw XYZ mismatch");

        constexpr std::array<std::uint32_t, 6> invalidFactors{
            0x2f7fffffu, 0xaf800000u, 0x3f800001u, 0x00000001u, 0x7f800000u, 0x7fc12345u};
        for (const auto factor : invalidFactors)
        {
            auto output = Marker;
            Require(!TryInterpolateAnimationVectorLinearPcForAnalysis(LeftWitness, RightWitness, factor, output)
                        && output == Marker,
                    "invalid factor mutated the destination");
        }
        for (std::size_t axis = 0; axis < LeftWitness.size(); ++axis)
        {
            auto first = LeftWitness;
            first[axis] = 0x45800001u;
            const auto previous = first;
            Require(!TryInterpolateAnimationVectorLinearPcForAnalysis(first, RightWitness, 0x3f000000u, first)
                        && first == previous,
                    "invalid first coordinate mutated the alias");
            auto second = RightWitness;
            second[axis] = 0x7fc12345u;
            const auto secondPrevious = second;
            Require(!TryInterpolateAnimationVectorLinearPcForAnalysis(LeftWitness, second, 0x3f000000u, second)
                        && second == secondPrevious,
                    "invalid second coordinate mutated the alias");
        }
        std::cout << "AnimationVectorLinearPc: " << checks << " contracts passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        if (argc != 1)
            throw std::runtime_error("unknown animation interpolation command");
        return RunTests();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
