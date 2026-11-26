#include "Code/Sparkplug/spFunctionEvalLinearRampPc.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::FunctionEvalLinearRampPc80ForAnalysis;
    using sparkplug::reconstruction::FunctionEvalLinearRampPcInputForAnalysis;
    using sparkplug::reconstruction::TryEvaluateFunctionEvalLinearRampPcForAnalysis;

    using Words = std::array<std::uint32_t, 5>;

    struct Fixture
    {
        const char* name;
        Words input;
        std::string_view expected;
    };

    constexpr std::array<Fixture, 14> Planned{{
        {"positive-before-clamp", {0x3e800000u, 0x40000000u, 0x40400000u, 0x40800000u, 0x01u}, "00000000000000e00040"},
        {"positive-equal-clamps", {0x3f000000u, 0x40000000u, 0x40400000u, 0x40800000u, 0x01u}, "00000000000000800140"},
        {"positive-beyond-clamp", {0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u, 0xffu}, "00000000000000800140"},
        {"negative-below-clamp", {0x3f800000u, 0xc0000000u, 0x40400000u, 0x3fc00000u, 0x01u}, "00000000000000c0ff3f"},
        {"negative-equal-clamps", {0x3f800000u, 0xc0000000u, 0x40400000u, 0x3f800000u, 0x01u}, "0000000000000080ff3f"},
        {"negative-above-clamp", {0x3f000000u, 0xc0000000u, 0x40400000u, 0x3f800000u, 0x01u}, "00000000000000800040"},
        {"zero-pitch-disables-directional-clamp", {0x3f800000u, 0x00000000u, 0x40400000u, 0x3f800000u, 0x01u}, "00000000000000c00040"},
        {"negative-zero-pitch-and-offset", {0x3f800000u, 0x80000000u, 0x80000000u, 0x3f800000u, 0x80u}, "00000000000000000080"},
        {"negative-zero-time-product-keeps-sign", {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x00u}, "00000000000000000080"},
        {"extended-midpoint-f32-distinction", {0x3c28e840u, 0x42c20000u, 0x23800000u, 0x00000000u, 0x00u}, "8000000080000080ff3f"},
        {"positive-just-below-limit", {0x3f800000u, 0x3f800000u, 0xa3800000u, 0x3f800000u, 0x01u}, "00fffffffffffffffe3f"},
        {"negative-just-below-limit", {0x3f800000u, 0xbf800000u, 0xa3800000u, 0xbf800000u, 0x01u}, "0000000000000080ffbf"},
        {"least-normal-product-retained", {0x00800000u, 0x00800000u, 0x00000000u, 0x00000000u, 0x00u}, "0000000000000080033f"},
        {"negative-equal-signed-zero-clamps", {0x00000000u, 0xbf800000u, 0x00000000u, 0x80000000u, 0x01u}, "00000000000000000080"},
    }};

    constexpr std::array<Fixture, 1> SignedZeroEquality{{
        {"positive equality loads negative-zero limit", {0u, 0x3f800000u, 0u, 0x80000000u, 0x80u}, "00000000000000000080"},
    }};

    constexpr FunctionEvalLinearRampPc80ForAnalysis Marker{0xA0, 0x94, 0x20, 0xE7, 0x5B, 0x04, 0x19, 0x7C, 0xD1, 0x4B};

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    FunctionEvalLinearRampPcInputForAnalysis Input(const Words& words) noexcept
    {
        return {words[0], words[1], words[2], words[3], static_cast<std::uint8_t>(words[4])};
    }

    std::string Hex(const FunctionEvalLinearRampPc80ForAnalysis& value)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(value.size() * 2);
        for (const auto byte : value)
        {
            result += digits[byte >> 4];
            result += digits[byte & 15u];
        }
        return result;
    }

    Words Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        Words words{};
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            std::string field;
            const auto width = i == words.size() - 1 ? 2u : 8u;
            if (!(fields >> field) || field.size() != width || field.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("expected four raw binary32 words and one clamp byte");
            const auto end = field.data() + field.size();
            const auto parsed = std::from_chars(field.data(), end, words[i], 16);
            if (parsed.ec != std::errc{} || parsed.ptr != end)
                throw std::runtime_error("invalid FunctionEval ramp word");
        }
        std::string extra;
        if (fields >> extra)
            throw std::runtime_error("unexpected FunctionEval ramp field");
        return words;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            auto value = Marker;
            Require(TryEvaluateFunctionEvalLinearRampPcForAnalysis(Input(Parse(line)), value), "ramp fixture refused");
            std::cout << Hex(value) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "ramp replay stream failure");
        return 0;
    }

    void Check(const Fixture& fixture)
    {
        auto value = Marker;
        Require(TryEvaluateFunctionEvalLinearRampPcForAnalysis(Input(fixture.input), value)
                && Hex(value) == fixture.expected, fixture.name);
    }

    int RunTests()
    {
        for (const auto& fixture : Planned)
            Check(fixture);
        for (const auto& fixture : SignedZeroEquality)
            Check(fixture);

        // Both sides of the raw magnitude guard and invalid encodings on all fields.
        constexpr std::array<std::uint32_t, 6> invalid{0x46800001u, 0xC6800001u, 0x00000001u,
                                                       0x807FFFFFu, 0x7F800000u, 0xFFC12345u};
        for (std::size_t field = 0; field < 4; ++field)
        {
            for (const auto bits : invalid)
            {
                auto input = Planned[0].input;
                input[field] = bits;
                auto value = Marker;
                Require(!TryEvaluateFunctionEvalLinearRampPcForAnalysis(Input(input), value) && value == Marker,
                        "unqualified ramp input modified the result");
            }
        }

        const Fixture maximum{"both signed maximum inputs admitted", {0x46800000u, 0xc6800000u, 0u, 0u, 0u},
                              "00000000000000801bc0"};
        Check(maximum);
        std::cout << "FunctionEvalLinearRampPc: " << checks << " contracts passed\n";
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
