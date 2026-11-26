#include "Code/Sparkplug/spPlaneFromThree.h"

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
    using sparkplug::reconstruction::BuildPlaneFromThreePcNearestForAnalysis;
    using Point = std::array<std::uint32_t, 3>;
    using Points = std::array<Point, 3>;
    using Plane = std::array<std::uint32_t, 4>;

    struct Witness final
    {
        std::string_view points;
        std::string_view plane;
    };

    constexpr std::array<Witness, 14> Witnesses{{
        {"000000000000000000004040 0000803f0000000000004040 000000000000803f00004040", "00000000000000000000803f00004040"},
        {"000000000000000000004040 000000000000803f00004040 0000803f0000000000004040", "0000000000000000000080bf000040c0"},
        {"00004040000000c00000a040 00008040000040400000803f 000000c0000080bf00004040", "89a831be7eda223f9576403fc3aefb3f"},
        {"000080400000404000000040 0000a0400000804000004040 0000c0400000a04000008040", "00000000000000000000000000000000"},
        {"000000800000008000000080 000000000000000000000000 000000000000000000000000", "00000000000000000000000000000080"},
        {"000000000000000000000000 00000000000000000000803f 000000006e1283ba00000000", "00000000000000000000000000000000"},
        {"000000000000000000000000 00000000000000000000803f 000000006f1283ba00000000", "00000000000000000000000000000000"},
        {"000000000000000000000000 00000000000000000000803f 00000000701283ba00000000", "0000803f000000000000008000000000"},
        {"000000000000000000000000 00000000000000000000803f 0000802c6f1283ba00000000", "0000803fffff79310000008000000000"},
        {"000000800000000000000000 00000000000000000000803f 6f1283ba0000802c00000000", "ffff79b1000080bf0000000000000000"},
        {"000000000000000000000000 000000000000c03f00008031 00000040000080310100803f", "2ff9e43e1fa618312ef964bf00000000"},
        {"000000000000000000000000 000000000000c03f000080b1 00000040000080310300803f", "33f9e43e1ea618b12df964bf00000000"},
        {"000080c100000000000080c1 0000804100000080000080c1 000080c10000802b00008041", "00000080000080bf00000029000000ab"},
        {"000040c0000000c0000080bf 000000c0000000c0000080bf 000040c0000080bf000080bf", "00000000000000000000803f000080bf"},
    }};

    constexpr Plane Dirty{0xA1B2C3D4u, 0xFF800000u, 0x7FA12345u, 0xFAE8231Du};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Points Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        Points points{};
        for (auto& point : points)
        {
            std::string raw;
            Require(bool(fields >> raw) && raw.size() == 24, "expected a 12-byte point");
            for (std::size_t lane = 0; lane < point.size(); ++lane)
            {
                for (unsigned byte = 0; byte != 4; ++byte)
                {
                    unsigned value = 0;
                    const auto first = raw.data() + 8*lane + 2*byte;
                    const auto parsed = std::from_chars(first, first + 2, value, 16);
                    Require(parsed.ec == std::errc{} && parsed.ptr == first + 2, "invalid point hexadecimal byte");
                    point[lane] |= std::uint32_t(value) << (8*byte);
                }
            }
        }
        std::string extra;
        Require(!(fields >> extra), "expected exactly three points");
        return points;
    }

    std::string Encode(const Plane& plane)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        for (const auto word : plane)
            for (unsigned byte = 0; byte != 4; ++byte)
                raw << std::setw(2) << static_cast<unsigned>(word >> (8*byte) & 0xFFu);
        return raw.str();
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            auto plane = Dirty;
            Require(BuildPlaneFromThreePcNearestForAnalysis(Parse(line), plane), "qualified plane fixture was refused");
            std::cout << Encode(plane) << '\n';
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
        for (const auto& witness : Witnesses)
        {
            auto plane = Dirty;
            check(BuildPlaneFromThreePcNearestForAnalysis(Parse(witness.points), plane), "planned native fixture was refused");
            check(Encode(plane) == witness.plane, "normal XYZ or plane distance differs");
        }

        const auto refuse = [&](const Points& points)
        {
            auto plane = Dirty;
            check(!BuildPlaneFromThreePcNearestForAnalysis(points, plane), "unqualified plane computed");
            check(plane == Dirty, "refused plane modified destination");
        };
        constexpr std::uint32_t Tiny = 0x2B800000u;
        constexpr std::uint32_t One = 0x3F800000u;
        refuse(Points{{Point{}, Point{0, 0, Tiny}, Point{Tiny, 0, 0}}});
        refuse(Points{{Point{}, Point{0, 0, Tiny}, Point{Tiny, One, 0}}});
        constexpr Points Usable{{{}, Point{One, 0, 0}, Point{0, One, 0}}};
        constexpr std::array<std::uint32_t, 8> Outside{
            0x807FFFFFu, 0x2B7FFFFFu, 0x41800001u, 0xC1800001u,
            0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u,
        };
        for (const auto bad : Outside)
            for (std::size_t point = 0; point < Usable.size(); ++point)
                for (std::size_t lane = 0; lane < Usable[point].size(); ++lane)
                {
                    auto invalid = Usable;
                    invalid[point][lane] = bad;
                    refuse(invalid);
                }
        std::cout << "PlaneFromThree: " << checks << " checks passed\n";
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
