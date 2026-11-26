#include "Code/Sparkplug/spOrientationBasis.h"

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
    using sparkplug::reconstruction::BuildOrientationBasisPcNearestForAnalysis;
    using Vector = std::array<std::uint32_t, 3>;
    using Basis = std::array<std::uint32_t, 9>;

    struct Witness final
    {
        std::string_view forward;
        std::string_view up;
        std::string_view basis;
    };

    constexpr std::array<Witness, 12> Witnesses{{
        {"000080bf0000000000000000", "000000000000803f00000000", "00000000000000800000803f000000000000803f00000000000080bf0000000000000000"},
        {"00000000000080bf00000000", "000000000000803f00000000", "000000000000000000000080000000000000803f0000000000000000000080bf00000000"},
        {"000000400000000000000000", "000000000000404000000000", "00000000000000000000c0c0000000000000404000000000000000400000000000000000"},
        {"7012833a0000008000000000", "000000000000803f0000803f", "000000007012833a701283ba000000000000803f0000803f7012833a0000008000000000"},
        {"6f12833a7012833a00000000", "00000000000000000000803f", "701283ba6f12833a0000000000000000000000000000803f6f12833a7012833a00000000"},
        {"6e12833a6f12833a7012833a", "000000000000008000000000", "0000008000000000000000000000000000000080000000006e12833a6f12833a7012833a"},
        {"0000803f000000000000803f", "000000800000008000000000", "0000008000000000000000000000008000000080000000000000803f000000000000803f"},
        {"00000040000080310100803f", "000000000000c03f00008031", "0100c03f00000032000040c0000000000000c03f0000803100000040000080310100803f"},
        {"00000040000080310300803f", "000000000000c03f000080b1", "0500c03f000000b2000040c0000000000000c03f000080b100000040000080310300803f"},
        {"000080c50000802f00008045", "00008045000080af00008045", "000000b6000000cc0000000000008045000080af00008045000080c50000802f00008045"},
        {"00000040000080c00000c040", "0000803f000000c000004040", "0000000000000000000000000000803f000000c00000404000000040000080c00000c040"},
        {"00000080000040c000000041", "000000000000803f000000c0", "000000400000000000000000000000000000803f000000c000000080000040c000000041"},
    }};

    constexpr Basis Dirty{0xA1B2C3D4u, 0xFF800000u, 0x7FA12345u, 0xFAE8231Du, 0xFFFFFFFFu,
                          0xFEEFAA33u, 0x80000000u, 0x77772345u, 0xB9991223u};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Vector ParseVector(std::string_view raw)
    {
        Require(raw.size() == 24, "expected a 12-byte direction vector");
        Vector output{};
        for (std::size_t lane = 0; lane < output.size(); ++lane)
        {
            for (unsigned byte = 0; byte != 4; ++byte)
            {
                unsigned value = 0;
                const auto first = raw.data() + 8*lane + 2*byte;
                const auto parsed = std::from_chars(first, first + 2, value, 16);
                Require(parsed.ec == std::errc{} && parsed.ptr == first + 2, "invalid hexadecimal byte");
                output[lane] |= std::uint32_t(value) << (8*byte);
            }
        }
        return output;
    }

    std::string Encode(const Basis& basis)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        for (const auto word : basis)
            for (unsigned byte = 0; byte != 4; ++byte)
                raw << std::setw(2) << static_cast<unsigned>(word >> (8*byte) & 0xFFu);
        return raw.str();
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            std::string forward, up, excess;
            Require(bool(input >> forward >> up) && !(input >> excess), "expected forward and up hexadecimal directions");
            auto basis = Dirty;
            Require(BuildOrientationBasisPcNearestForAnalysis(ParseVector(forward), ParseVector(up), basis),
                "qualified original fixture was refused");
            std::cout << Encode(basis) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    int RunTests()
    {
        unsigned checks = 0;
        const auto check = [&](bool result, const char* message)
        {
            Require(result, message);
            ++checks;
        };
        for (const auto& item : Witnesses)
        {
            auto basis = Dirty;
            check(BuildOrientationBasisPcNearestForAnalysis(ParseVector(item.forward), ParseVector(item.up), basis),
                "planned original fixture was refused");
            check(Encode(basis) == item.basis, "raw orientation basis differs");
        }

        const auto refuse = [&](const Vector& forward, const Vector& up)
        {
            auto basis = Dirty;
            check(!BuildOrientationBasisPcNearestForAnalysis(forward, up, basis), "unqualified basis computed");
            check(basis == Dirty, "refused basis modified destination");
        };
        refuse({0, 0x3F800000u, 0}, {0, 0x3F800000u, 0});
        refuse({0x3A03126Fu, 0x3F800000u, 0}, {0, 0x3F800000u, 0});
        refuse({0x3A83126Fu, 0xBA83126Fu, 0x3A83126Fu}, {});
        refuse({0x80000000u, 0, 0x80000000u}, {});
        constexpr Vector Usable{0x40000000u, 0x3F800000u, 0x80000000u};
        constexpr std::array<std::uint32_t, 8> Outside{
            0x807FFFFFu, 0x2F7FFFFFu, 0x45800001u, 0xC5800001u,
            0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u,
        };
        for (const auto bad : Outside)
        {
            for (std::size_t lane = 0; lane < Usable.size(); ++lane)
            {
                auto invalid = Usable;
                invalid[lane] = bad;
                refuse(invalid, {});
                refuse({}, invalid);
            }
        }
        std::cout << "OrientationBasis: " << checks << " checks passed\n";
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
