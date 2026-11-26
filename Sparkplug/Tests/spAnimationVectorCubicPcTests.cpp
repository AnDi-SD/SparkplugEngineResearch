#include "Code/Sparkplug/spAnimationVectorCubicPc.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::AnimationVectorCubicPcRecordForAnalysis;
    using sparkplug::reconstruction::AnimationVectorCubicPcWordsForAnalysis;
    using sparkplug::reconstruction::TrySampleAnimationVectorCubicPcForAnalysis;
    using Record = AnimationVectorCubicPcRecordForAnalysis;
    using Vector = AnimationVectorCubicPcWordsForAnalysis;

    enum class Destination { Separate, Value, Quadratic, Cubic };

    struct Fixture
    {
        Destination destination;
        std::uint32_t factor;
        Record record;
        Vector expected;
    };

    constexpr Vector Repeat(std::uint32_t word) { return {word, word, word}; }

    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr std::uint32_t Half = 0x3f000000u;
    constexpr std::uint32_t One = 0x3f800000u;
    constexpr Vector Incoming{0x7fc00001u, 0x7f800000u, 0x2f700001u};
    constexpr Vector Marker{0xfef10234u, 0xc1caf123u, 0x7fc12345u};
    constexpr Record Typical{{0x3f800000u, 0x40000000u, 0x40400000u}, Incoming,
                             {0x40800000u, 0x40a00000u, 0x40c00000u},
                             {0xbf800000u, 0xc0000000u, 0xc0400000u},
                             {Half, One, 0x3fc00000u}};
    constexpr Vector TypicalHalfOut{0x40340000u, 0x40840000u, 0x40ae0000u};
    constexpr Record Discriminator{Repeat(0xbfa71d40u), Incoming, Repeat(0x407f5a59u),
                                   Repeat(0xc07268c0u), Repeat(0x3f6281bdu)};
    constexpr std::uint32_t DiscriminatorFactor = 0x3f65f2d1u;
    constexpr Vector DiscriminatorOut{0xbe0bbdd7u, 0xbe0bbdd8u, 0xbe0bbdd0u};
    constexpr Record ZeroSigns{{NegativeZero, 0, NegativeZero}, Incoming,
                               {NegativeZero, NegativeZero, 0}, {NegativeZero, 0, NegativeZero},
                               {NegativeZero, 0, NegativeZero}};
    constexpr Record Cancellation{{One, 0xbf800000u, NegativeZero}, Incoming,
                                  {0xbf800000u, One, NegativeZero}, {0, 0, NegativeZero}, {0, 0, NegativeZero}};
    constexpr Vector TinyBase{0, NegativeZero, 0};
    constexpr Record Tiny{TinyBase, TinyBase, TinyBase, TinyBase, {0x2f800000u, 0xaf800000u, 0x2f800000u}};
    constexpr std::array<Fixture, 10> Fixtures{{
        {Destination::Separate, Half, Typical, TypicalHalfOut},
        {Destination::Separate, DiscriminatorFactor, Discriminator, DiscriminatorOut},
        {Destination::Separate, 0, ZeroSigns, {NegativeZero, 0, 0}},
        {Destination::Separate, NegativeZero, ZeroSigns, {0, 0, NegativeZero}},
        {Destination::Separate, One, Typical, {0x40900000u, 0x40c00000u, 0x40f00000u}},
        {Destination::Separate, One, Cancellation, {0, 0, NegativeZero}},
        {Destination::Separate, 0x2f800000u, Tiny, {0x00200000u, 0x80200000u, 0x00200000u}},
        {Destination::Value, DiscriminatorFactor, Discriminator, DiscriminatorOut},
        {Destination::Quadratic, Half, Typical, TypicalHalfOut},
        {Destination::Cubic, DiscriminatorFactor, Discriminator, DiscriminatorOut},
    }};

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    Vector& SelectOutput(Destination destination, Record& record, Vector& separate)
    {
        switch (destination)
        {
        case Destination::Value: return record.value;
        case Destination::Quadratic: return record.quadratic;
        case Destination::Cubic: return record.cubic;
        case Destination::Separate: return separate;
        }
        throw std::runtime_error("unknown destination selection");
    }

    Vector Evaluate(const Fixture& fixture)
    {
        auto record = fixture.record;
        auto separate = Marker;
        auto& output = SelectOutput(fixture.destination, record, separate);
        Require(TrySampleAnimationVectorCubicPcForAnalysis(record, fixture.factor, output),
                "packed cubic animation fixture refused");
        Require((fixture.destination == Destination::Value || record.value == fixture.record.value)
                    && (fixture.destination == Destination::Quadratic || record.quadratic == fixture.record.quadratic)
                    && (fixture.destination == Destination::Cubic || record.cubic == fixture.record.cubic)
                    && record.incomingTangent == fixture.record.incomingTangent
                    && record.outgoingTangent == fixture.record.outgoingTangent
                    && (fixture.destination == Destination::Separate || separate == Marker),
                "cubic animation sampler changed a non-destination group");
        return output;
    }

    std::uint32_t ReadWord(std::istringstream& fields)
    {
        std::string field;
        if (!(fields >> field) || field.size() != 8 ||
            field.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("expected a raw binary32 word");
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
        if (destination == "value")
            fixture.destination = Destination::Value;
        else if (destination == "quadratic")
            fixture.destination = Destination::Quadratic;
        else if (destination == "cubic")
            fixture.destination = Destination::Cubic;
        else if (destination != "separate")
            throw std::runtime_error("unknown destination selection");
        fixture.factor = ReadWord(fields);
        auto& record = fixture.record;
        for (auto* vector : {&record.value, &record.incomingTangent, &record.outgoingTangent, &record.quadratic, &record.cubic})
            for (auto& word : *vector)
                word = ReadWord(fields);
        if (fields >> destination)
            throw std::runtime_error("unexpected cubic animation fixture field");
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
        Require(std::cin.eof() && bool(std::cout), "cubic animation replay stream failed");
        return 0;
    }

    int RunTests()
    {
        for (const auto& fixture : Fixtures)
            Require(Evaluate(fixture) == fixture.expected, "packed cubic animation raw XYZ mismatch");

        for (const auto factor : {0x2f7fffffu, 0xaf800000u, 0x3f800001u, 0x00000001u, 0x7f800000u, 0x7fc12345u})
        {
            auto record = Typical;
            Require(!TrySampleAnimationVectorCubicPcForAnalysis(record, factor, record.value)
                        && record.value == Typical.value,
                    "invalid factor mutated the destination alias");
        }
        for (const auto group : {&Record::value, &Record::outgoingTangent, &Record::quadratic, &Record::cubic})
        {
            auto record = Typical;
            auto& output = record.*group;
            output[1] = 0x45800001u;
            const auto before = output;
            Require(!TrySampleAnimationVectorCubicPcForAnalysis(record, Half, output) && output == before,
                    "invalid used coordinate mutated the destination alias");
        }
        std::cout << "AnimationVectorCubicPc: " << checks << " contracts passed\n";
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
            throw std::runtime_error("unknown cubic animation command");
        return RunTests();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
