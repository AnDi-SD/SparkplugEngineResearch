#include "Code/Sparkplug/spMatrix4ProductPc.h"

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
    using sparkplug::reconstruction::MultiplyMatrix4PcNearestForAnalysis;
    using Matrix = std::array<std::uint32_t, 16>;

    struct Witness final
    {
        std::string_view left;
        std::string_view right;
        std::string_view output;
    };

    // Independent nearest-even model of the decoded PC cell schedules.
    constexpr std::array<Witness, 8> Witnesses{{
        // noncommuting-skin-fixture
        {"0000803f000000000000000000000000000000000000803f0000000000000000"
         "00000000000000000000803f000000000000a0400000c0400000e0400000803f",
         "0000004000000000000000000000000000000000000040400000000000000000"
         "000000000000000000008040000000000000803f00000040000040400000803f",
         "0000004000000000000000000000000000000000000040400000000000000000"
         "00000000000000000000804000000000000030410000a0410000f8410000803f"},
        // precision-64-vs-53
        {"000080bf000080210000803f0000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f"
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f",
         "0000802100008021000080210000802100000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
        // native-order-tie
        {"000080bf0000801f0000803f0000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f"
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f",
         "0000000000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
        // every-product-negative-zero
        {"0000008000000080000000800000008000000080000000800000008000000080"
         "0000008000000080000000800000008000000080000000800000008000000080",
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f"
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f",
         "0000008000000080000000800000008000000080000000800000008000000080"
         "0000008000000080000000800000008000000080000000800000008000000080"},
        // mixed-zero-sign
        {"0000008000000000000000800000008000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f"
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f",
         "0000000000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
        // destination-f32-tie
        {"00000000000080330000803f0000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f"
         "0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f",
         "0000803f0000803f0000803f0000803f00000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
        // destination-subnormal
        {"0000801f00000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000801f00000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000200000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
        // destination-negative-zero-underflow
        {"0000809f0000801f000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0100801f0000000000000000000000000000801f000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000",
         "0000008000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"},
    }};

    constexpr Matrix Dirty{0xF4E835CDu, 0x281B0A01u, 0x5C4F3E35u, 0x9B7D6F20u,
                           0x90FE2158u, 0xC7AF413Au, 0x417B1291u, 0xD41CE783u,
                           0x38283725u, 0x88665544u, 0xEE55CC33u, 0x139AA6CDu,
                           0x770FA19Bu, 0x2CB61AE5u, 0xA2A7E11Fu, 0x93719687u};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Matrix ParseHex(std::string_view text)
    {
        Require(text.size() == 128, "expected sixteen raw matrix words");
        Matrix words{};
        for (std::size_t cell = 0; cell < words.size(); ++cell)
            for (unsigned byte = 0; byte < 4; ++byte)
            {
                unsigned value = 0;
                const auto begin = text.data() + 8 * cell + 2 * byte;
                const auto read = std::from_chars(begin, begin + 2, value, 16);
                Require(read.ec == std::errc{} && read.ptr == begin + 2, "invalid hexadecimal byte");
                words[cell] |= std::uint32_t(value) << (8 * byte);
            }
        return words;
    }

    std::string Encode(const Matrix& matrix)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        for (const auto word : matrix)
            for (unsigned byte = 0; byte < 4; ++byte)
                raw << std::setw(2) << static_cast<unsigned>(word >> (8 * byte) & 0xFFu);
        return raw.str();
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string left, right, extra;
            Require(bool(fields >> left >> right) && !(fields >> extra), "expected left and right matrices");
            auto output = Dirty;
            Require(MultiplyMatrix4PcNearestForAnalysis(ParseHex(left), ParseHex(right), output),
                "qualified replay matrices refused");
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
            const auto left = ParseHex(row.left);
            const auto right = ParseHex(row.right);
            auto output = Dirty;
            check(MultiplyMatrix4PcNearestForAnalysis(left, right, output), "qualified matrices refused");
            check(Encode(output) == row.output, "matrix product differs from the PC model");
            check(Encode(left) == row.left && Encode(right) == row.right, "product modified an input matrix");
        }

        const auto left = ParseHex(Witnesses[0].left);
        const auto right = ParseHex(Witnesses[0].right);
        constexpr std::array<std::uint32_t, 10> Outside{
            0x00000001u, 0x807FFFFFu, 0x1F7FFFFFu, 0x9F7FFFFFu, 0x4F800001u,
            0xCF800001u, 0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u};
        for (const auto word : Outside)
            for (std::size_t cell = 0; cell < left.size(); ++cell)
            {
                auto invalidLeft = left;
                invalidLeft[cell] = word;
                auto output = Dirty;
                check(!MultiplyMatrix4PcNearestForAnalysis(invalidLeft, right, output), "unqualified left word accepted");
                check(output == Dirty, "refused left matrix modified the output");

                auto invalidRight = right;
                invalidRight[cell] = word;
                check(!MultiplyMatrix4PcNearestForAnalysis(left, invalidRight, output), "unqualified right word accepted");
                check(output == Dirty, "refused right matrix modified the output");
            }

        auto output = Dirty;
        check(!MultiplyMatrix4PcNearestForAnalysis(left, left, output), "input alias accepted");
        check(output == Dirty, "input alias modified the output");
        auto aliasedLeft = left;
        check(!MultiplyMatrix4PcNearestForAnalysis(aliasedLeft, right, aliasedLeft), "left output alias accepted");
        check(aliasedLeft == left, "left output alias modified its matrix");
        auto aliasedRight = right;
        check(!MultiplyMatrix4PcNearestForAnalysis(left, aliasedRight, aliasedRight), "right output alias accepted");
        check(aliasedRight == right, "right output alias modified its matrix");

        const Matrix endpoints{0x4F800000u, 0, 0, 0, 0, 0xCF800000u, 0, 0,
                               0, 0, 0x1F800000u, 0, 0, 0, 0, 0x9F800000u};
        const auto sameWords = endpoints;
        const Matrix squared{0x5F800000u, 0, 0, 0, 0, 0x5F800000u, 0, 0,
                             0, 0, 0x00200000u, 0, 0, 0, 0, 0x00200000u};
        check(MultiplyMatrix4PcNearestForAnalysis(endpoints, sameWords, output), "inclusive endpoints refused");
        check(output == squared, "inclusive endpoints produced an incorrect matrix");

        std::cout << "Matrix4ProductPc: " << checks << " checks passed\n";
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
