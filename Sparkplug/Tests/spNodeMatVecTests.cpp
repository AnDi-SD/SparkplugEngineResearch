#include "Code/Sparkplug/spNodeMatVec.h"

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
    using sparkplug::reconstruction::TransformNodeVectorPcNearestForAnalysis;
    using Vector = std::array<std::uint32_t, 3>;

    struct Witness final
    {
        std::string_view input;
        std::string_view output;
    };

    constexpr std::array<Witness, 12> Witnesses{{
        {"0100803f0100803f0100803f000080310000803100008031000000000000000000000000 0000c03f000080b100000000", "0100c03f0100c03f0100c03f"},
        {"0300803f0300803f0300803f000080310000803100008031000000000000000000000000 0000c03f0000803100000000", "0500c03f0500c03f0500c03f"},
        {"0100803f0100803f0100803f000080310000803100008031000000000000000000000000 0000c0bf0000803100000000", "0100c0bf0100c0bf0100c0bf"},
        {"0300803f0300803f0300803f000080310000803100008031000000000000000000000000 0000c0bf000080b100000000", "0500c0bf0500c0bf0500c0bf"},
        {"0000000000008045000080c500000000000080c500008045000000000000802f000080af 00008045000080450000802f", "000000000000000000000000"},
        {"0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f 000000800000008000000080", "000000800000008000000080"},
        {"0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f 20bcbe4c0000803f20bcbecc", "0000803f0000803f0000803f"},
        {"000000800000008000000080000000800000008000000080000000800000008000000080 000000000000008000000000", "000000000000000000000000"},
        {"d8027543b9bd1cc09c1dbfbda8aa34c1adc23ebe773089bf3e3a193b4672c7bbe564fbc1 5bd0d8419428cc409256c23a", "7d41cd452d2087c2579e16c1"},
        {"b6a9243d84b15f3fac1ff83e189fa03da0f8f8bebaca5e3ffe007f3f1a06463b1463b4bd 6fcf75c379ae4b43bf7a2d42", "ca2c454202da9cc3fa435942"},
        {"0000004d000000cd0000004d0000004d0000004d000000cd000080af0000802f00000080 0000004d000000cd0000802f", "00000000000000db0000005b"},
        {"000000000000803f00000000000080bf000000000000000000000000000000000000803f 0000803f0000004000004040", "000000c00000803f00004040"},
    }};

    constexpr Vector Dirty{0xF4E835CDu, 0x281B0A01u, 0x5C4F3E35u};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <std::size_t WordCount>
    std::array<std::uint32_t, WordCount> ParseHex(std::string_view text)
    {
        Require(text.size() == WordCount*8, "incorrect raw word count");
        std::array<std::uint32_t, WordCount> words{};
        for (std::size_t lane = 0; lane < WordCount; ++lane)
            for (unsigned byte = 0; byte != 4; ++byte)
            {
                unsigned value = 0;
                const auto begin = text.data() + 8*lane + 2*byte;
                const auto read = std::from_chars(begin, begin + 2, value, 16);
                Require(read.ec == std::errc{} && read.ptr == begin + 2, "invalid hexadecimal byte");
                words[lane] |= std::uint32_t(value) << (8*byte);
            }
        return words;
    }

    struct Inputs final
    {
        std::array<std::uint32_t, 9> matrix;
        Vector vector;
    };

    Inputs Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        std::string matrix, vector, extra;
        Require(bool(fields >> matrix >> vector) && !(fields >> extra), "expected matrix and vector");
        return {ParseHex<9>(matrix), ParseHex<3>(vector)};
    }

    std::string Encode(const Vector& vector)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        for (const auto word : vector)
            for (unsigned byte = 0; byte != 4; ++byte)
                raw << std::setw(2) << static_cast<unsigned>(word >> (8*byte) & 0xFFu);
        return raw.str();
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            auto input = Parse(line);
            auto output = Dirty;
            Require(TransformNodeVectorPcNearestForAnalysis(input.matrix, input.vector, output), "qualified replay vector refused");
            std::cout << Encode(output) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    int RunTests()
    {
        unsigned checks = 0;
        const auto check = [&](bool condition, const char* message)
        {
            Require(condition, message);
            ++checks;
        };
        for (const auto& row : Witnesses)
        {
            auto input = Parse(row.input);
            const auto before = input;
            auto output = Dirty;
            check(TransformNodeVectorPcNearestForAnalysis(input.matrix, input.vector, output), "native vector refused");
            check(Encode(output) == row.output, "matrix-vector product differs from original");
            check(input.matrix == before.matrix && input.vector == before.vector,
                "transform modified unrelated words");
        }
        const auto usable = Parse(Witnesses[0].input);
        constexpr std::array<std::uint32_t, 8> Outside{0x807FFFFFu, 0x2F7FFFFFu, 0x4D000001u, 0xCD000001u,
                                                        0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u};
        for (const auto word : Outside)
        {
            for (std::size_t lane = 0; lane < usable.matrix.size(); ++lane)
            {
                auto matrix = usable.matrix;
                matrix[lane] = word;
                auto output = Dirty;
                check(!TransformNodeVectorPcNearestForAnalysis(matrix, usable.vector, output), "unqualified matrix word accepted");
                check(output == Dirty, "refused matrix modified the output");
            }
            for (std::size_t lane = 0; lane < usable.vector.size(); ++lane)
            {
                auto vector = usable.vector;
                vector[lane] = word;
                auto output = Dirty;
                check(!TransformNodeVectorPcNearestForAnalysis(usable.matrix, vector, output), "unqualified vector word accepted");
                check(output == Dirty, "refused vector modified the output");
            }
        }
        auto aliased = usable.vector;
        check(!TransformNodeVectorPcNearestForAnalysis(usable.matrix, aliased, aliased), "unqualified output alias accepted");
        check(aliased == usable.vector, "refused alias modified the input");
        std::cout << "NodeMatVec: " << checks << " checks passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunTests();
        Require(argc == 2 && std::string_view(argv[1]) == "--replay", "expected --replay or no arguments");
        return Replay();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
