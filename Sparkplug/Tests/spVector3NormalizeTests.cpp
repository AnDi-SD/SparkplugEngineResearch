#include "Code/Sparkplug/spVector3Normalization.h"

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
    using sparkplug::reconstruction::NormalizeVector3PcNearestForAnalysis;
    using sparkplug::reconstruction::spVector3Length80ForAnalysis;
    using Words = std::array<std::uint32_t, 3>;

    constexpr spVector3Length80ForAnalysis Marker{0xE4ACB21739715067ull, 0xB127u};

    struct Witness final
    {
        std::string_view before;
        std::string_view output;
    };

    constexpr std::array<Witness, 12> NativeWitnesses{{
        {"000040400000000000008040", "9a99193f00000000cdcc4c3f 00000000000000a00140"},
        {"000000800000000000000080", "000000000000000000000000 00000000000000000000"},
        {"6e12833a0000000000000000", "000000000000000000000000 00000000000000000000"},
        {"6f12833a0000008000000000", "000000000000000000000000 00000000000000000000"},
        {"7012833a0000008000000000", "0000803f0000008000000000 0000000000701283f53f"},
        {"6f12833a0000802c00000080", "0000803fffff793100000080 3e000000006f1283f53f"},
        {"000000806f1283ba000080ac", "00000080000080bfffff79b1 3e000000006f1283f53f"},
        {"000040c000008040000040c1", "c54e6cbed9899d3ec54e6cbf 00000000000000d00240"},
        {"00008045000080a7000080c5", "f304353ff30435a1f30435bf 8464def933f304b50b40"},
        {"00008027000080a700008027", "000000000000000000000000 00000000000000000000"},
        {"000080c50000008000000080", "000080bf0000008000000080 00000000000000800b40"},
        {"00000080000040c0000080c0", "000000809a9919bfcdcc4cbf 00000000000000a00140"},
    }};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Words ParseVector(std::string_view raw)
    {
        Require(raw.size() == 24, "expected 24 hexadecimal digits for XYZ");
        Words output{};
        for (std::size_t lane = 0; lane < output.size(); ++lane)
        {
            for (unsigned byte = 0; byte != 4; ++byte)
            {
                unsigned value = 0;
                const auto first = raw.data() + 8*lane + 2*byte;
                const auto converted = std::from_chars(first, first + 2, value, 16);
                Require(converted.ec == std::errc{} && converted.ptr == first + 2, "invalid XYZ hexadecimal byte");
                output[lane] |= std::uint32_t(value) << (8*byte);
            }
        }
        return output;
    }

    void PrintLittleEndian(std::ostream& output, std::uint64_t value, unsigned size)
    {
        for (unsigned byte = 0; byte != size; ++byte)
            output << std::setw(2) << static_cast<unsigned>(value >> (8*byte) & 0xFFu);
    }

    std::string Encode(const Words& vector, const spVector3Length80ForAnalysis& length)
    {
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (const auto word : vector)
            PrintLittleEndian(output, word, 4);
        output << ' ';
        PrintLittleEndian(output, length.significand, 8);
        PrintLittleEndian(output, length.signExponent, 2);
        return output.str();
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            auto vector = ParseVector(line);
            auto length = Marker;
            Require(NormalizeVector3PcNearestForAnalysis(vector, length), "replay vector outside PC profile");
            std::cout << Encode(vector, length) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    int RunTests()
    {
        unsigned checks = 0;
        const auto check = [&checks](bool result, const char* message)
        {
            Require(result, message);
            ++checks;
        };
        for (const auto& witness : NativeWitnesses)
        {
            auto vector = ParseVector(witness.before);
            auto length = Marker;
            check(NormalizeVector3PcNearestForAnalysis(vector, length), "native fixture was refused");
            check(Encode(vector, length) == witness.output, "vector DWORDs or raw ST0 differ from original");
        }

        constexpr Words Valid{0x40400000u, 0x80000000u, 0xC0800000u};
        constexpr std::array<std::uint32_t, 10> Outside{
            0x00000001u, 0x807FFFFFu, 0x00800000u, 0x277FFFFFu, 0x45800001u,
            0xC5800001u, 0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u,
        };
        for (const auto bad : Outside)
        {
            for (std::size_t lane = 0; lane < Valid.size(); ++lane)
            {
                auto before = Valid;
                before[lane] = bad;
                auto vector = before;
                auto length = Marker;
                check(!NormalizeVector3PcNearestForAnalysis(vector, length), "out-of-profile lane admitted");
                check(vector == before && length.significand == Marker.significand && length.signExponent == Marker.signExponent,
                    "out-of-profile vector or return value was modified");
            }
        }
        std::cout << "Vector3Normalize: " << checks << " checks passed\n";
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
