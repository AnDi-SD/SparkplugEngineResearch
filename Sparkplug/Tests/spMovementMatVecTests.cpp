#include "Code/Sparkplug/spMovementMatVec.h"

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
    using sparkplug::reconstruction::TransformMovementVectorPcNearestForAnalysis;
    using Vector = std::array<std::uint32_t, 3>;

    struct Witness final
    {
        std::string_view input;
        std::string_view output;
    };

    constexpr std::array<Witness, 12> Witnesses{{
        {"split 0000803f0000000000000000000000000000803f0000000000000000000000000000803f 000040400000a0c00000e040", "000040400000a0c00000e040"},
        {"alias 0000803f0000000000000000000000000000803f0000000000000000000000000000803f 000040400000a0c00000e040", "000040400000a0c00000e040"},
        {"split 0100803f0000803f0000000000008031000000000000000000000000000000000000803f 0000c03f000080b100000000", "0100c03f0000c03f00000000"},
        {"alias 0300803f0000803f0000000000008031000000000000000000000000000000000000803f 0000c03f0000803100000000", "0500c03f0000c03f00000000"},
        {"split 0000000000008045000080c500000000000080c500008045000000000000802f000080af 00008045000080450000802f", "000000000000801f0000809f"},
        {"split 0000802f0000000000000000000080c50000804500000000000080450000000000008045 0000802f0000804500008045", "0000801f0000804b0000804b"},
        {"alias 0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f 000000800000008000000080", "000000800000008000000080"},
        {"split 000000000000803f00000000000080bf000000000000000000000000000000000000803f 0000803f0000004000004040", "000000c00000803f00004040"},
        {"split 00008045000080c5000080450000804500008045000080c5000080af0000802f00000080 000080c5000000000000802f", "000080cb0000804b000080cb"},
        {"split 0000802f0000802f000080af0000802f000080af0000802f0000802f0000802f0000802f 0000802f000080af0000802f", "0000801f000040200000809f"},
        {"alias 00000040000080400000c0c0000080bf0000104100000041000080400000004000004040 000040c00000a0400000e040", "0000884100003c4200009e42"},
        {"split 0000803f0000000000000000000000000000803f0000000000000000000000000000803f 000000000000000000000000", "000000000000000000000000"},
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
        bool alias;
        std::array<std::uint32_t, 9> matrix;
        Vector vector;
    };

    Inputs Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        std::string mode, matrix, vector, extra;
        Require(bool(fields >> mode >> matrix >> vector) && !(fields >> extra), "expected mode, matrix and vector");
        Require(mode == "split" || mode == "alias", "expected split or alias");
        return {mode == "alias", ParseHex<9>(matrix), ParseHex<3>(vector)};
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
            auto& destination = input.alias ? input.vector : output;
            Require(TransformMovementVectorPcNearestForAnalysis(input.matrix, input.vector, destination), "qualified replay vector refused");
            std::cout << Encode(destination) << '\n';
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
            auto& destination = input.alias ? input.vector : output;
            check(TransformMovementVectorPcNearestForAnalysis(input.matrix, input.vector, destination), "native vector refused");
            check(Encode(destination) == row.output, "matrix-vector product differs from original");
            check(input.matrix == before.matrix && (input.alias ? output == Dirty : input.vector == before.vector),
                "transform modified unrelated words");
        }
        const auto usable = Parse(Witnesses[0].input);
        constexpr std::array<std::uint32_t, 8> Outside{0x807FFFFFu, 0x2F7FFFFFu, 0x45800001u, 0xC5800001u,
                                                        0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u};
        for (const auto word : Outside)
        {
            for (std::size_t lane = 0; lane < usable.matrix.size(); ++lane)
            {
                auto matrix = usable.matrix;
                matrix[lane] = word;
                auto output = Dirty;
                check(!TransformMovementVectorPcNearestForAnalysis(matrix, usable.vector, output), "unqualified matrix word accepted");
                check(output == Dirty, "refused matrix modified the output");
            }
            for (std::size_t lane = 0; lane < usable.vector.size(); ++lane)
            {
                auto vector = usable.vector;
                vector[lane] = word;
                const auto before = vector;
                check(!TransformMovementVectorPcNearestForAnalysis(usable.matrix, vector, vector), "unqualified vector word accepted");
                check(vector == before, "refused vector modified the aliased output");
            }
        }
        std::cout << "MovementMatVec: " << checks << " checks passed\n";
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
