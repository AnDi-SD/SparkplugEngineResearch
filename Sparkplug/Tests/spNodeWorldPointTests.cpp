#include "Code/Sparkplug/spNodeWorldPoint.h"

#include <algorithm>
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
    using sparkplug::reconstruction::spNodeWorldPointCacheForAnalysis;
    using sparkplug::reconstruction::TransformNodeWorldPointPcNearestForAnalysis;
    using Vector = std::array<std::uint32_t, 3>;

    struct Witness final
    {
        std::string_view inputs;
        std::string_view output;
    };

    constexpr std::array<Witness, 13> Witnesses{{
        {"split 000020410000a0410000f041000000400000404000008040000000000000803f00000000000080bf000000000000000000000000000000000000803f 0000803f0000004000004040", "000080400000b04100002842"},
        {"split 0000803f0000004000004040000000c000004040000080400000803f0000000000000000000000000000803f0000000000000000000000000000803f 000000c0000000c000002041", "0000a040000080c000002c42"},
        {"split 0000803f0000004000004040000000c000004040000080400000803f0000000000000000000000000000803f0000000000000000000000000000803f 00000040000000c000002041", "000040c0000080c000002c42"},
        {"split 0000803f0000004000004040000000c000004040000080400000803f0000000000000000000000000000803f0000000000000000000000000000803f 000000400000004000002041", "000040c00000004100002c42"},
        {"split 0000803f0000004000004040000000c000004040000080400000803f0000000000000000000000000000803f0000000000000000000000000000803f 000000c00000004000002041", "0000a0400000004100002c42"},
        {"split 00000000000000000000000000008037000080440000803f000080b700000000000000000100803f0000803f0000000000000000000000000000803f 000080370000c03f0000803f", "0100c0440000c0440000803f"},
        {"split 00000000000000000000000000008037000080440000803f0000803700000000000000000300803f0000803f0000000000000000000000000000803f 000080370000c03f0000803f", "0500c0440000c0440000803f"},
        {"split 000080c300000000000000000000803f0000803f0000803f000000000000000000000000000080370000803f0000000000008043000000000000803f 0000803f0000803f0000803f", "000000000000803f0000803f"},
        {"split 00000000000000000000000000008037000080450000804500008037000080c5000080c5000080c50000803700008037000080450000804500008045 000080370000804500008045", "000080270000805100008051"},
        {"split 0000008000000080000000800000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f0000803f 000000800000008000000080", "000000800000008000000080"},
        {"alias 000020410000a0410000f041000000400000404000008040000000000000803f00000000000080bf000000000000000000000000000000000000803f 0000803f0000004000004040", "000080400000b04100002842"},
        {"split 00008045000080c50000008000008045000080c500008037000080c50000004500008037000088c10000804000000041000080450000803f000080bf 000080c50000804500008037", "00888051014000d1100000cd"},
        {"split 00000000000000000000000000008045000080370000804500000000000080c5000080450000000000008037000080b70000000000008045000080c5 000080450000803700008045", "0000000000008027000080a7"},
    }};

    constexpr Vector Dirty{0xA1B2C3D4u, 0xFF800000u, 0x7FA12345u};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <std::size_t WordCount>
    std::array<std::uint32_t, WordCount> ParseHex(std::string_view text)
    {
        Require(text.size() == WordCount*8, "incorrect raw word count");
        std::array<std::uint32_t, WordCount> out{};
        for (std::size_t lane = 0; lane < WordCount; ++lane)
            for (unsigned byte = 0; byte != 4; ++byte)
            {
                unsigned value = 0;
                const auto begin = text.data() + 8*lane + 2*byte;
                const auto read = std::from_chars(begin, begin + 2, value, 16);
                Require(read.ec == std::errc{} && read.ptr == begin + 2, "invalid hexadecimal byte");
                out[lane] |= std::uint32_t(value) << (8*byte);
            }
        return out;
    }

    spNodeWorldPointCacheForAnalysis MakeCache(const std::array<std::uint32_t, 15>& raw)
    {
        spNodeWorldPointCacheForAnalysis cache{};
        std::copy_n(raw.begin(), 3, cache.positionBits.begin());
        std::copy_n(raw.begin() + 3, 3, cache.scaleBits.begin());
        std::copy_n(raw.begin() + 6, 9, cache.orientationBits.begin());
        return cache;
    }

    bool SameCache(const spNodeWorldPointCacheForAnalysis& a, const spNodeWorldPointCacheForAnalysis& b)
    {
        return a.positionBits == b.positionBits && a.scaleBits == b.scaleBits && a.orientationBits == b.orientationBits;
    }

    struct Inputs final
    {
        bool alias;
        spNodeWorldPointCacheForAnalysis cache;
        Vector point;
    };

    Inputs Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        std::string mode, cache, point, extra;
        Require(bool(fields >> mode >> cache >> point) && !(fields >> extra), "expected mode, cache and three input words");
        Require(mode == "split" || mode == "alias", "expected split or alias");
        return {mode == "alias", MakeCache(ParseHex<15>(cache)), ParseHex<3>(point)};
    }

    std::string Encode(const Vector& point)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        for (const auto word : point)
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
            auto& destination = input.alias ? input.point : output;
            Require(TransformNodeWorldPointPcNearestForAnalysis(input.cache, input.point, destination), "qualified replay point refused");
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
            auto input = Parse(row.inputs);
            const auto before = input;
            auto output = Dirty;
            auto& destination = input.alias ? input.point : output;
            check(TransformNodeWorldPointPcNearestForAnalysis(input.cache, input.point, destination), "native point refused");
            check(Encode(destination) == row.output, "world point differs from original");
            check(SameCache(input.cache, before.cache) && (input.alias ? output == Dirty : input.point == before.point),
                "world transform modified the protected input or unused output");
        }
        const auto usable = Parse(Witnesses[0].inputs);
        constexpr std::array<std::uint32_t, 8> Outside{0x807FFFFFu, 0x377FFFFFu, 0x45800001u, 0xC5800001u,
                                                        0x7F800000u, 0xFF800000u, 0x7FC12345u, 0xFFA12345u};
        const auto badFields = [&](auto field)
        {
            for (const auto word : Outside)
                for (std::size_t lane = 0; lane < field(usable).size(); ++lane)
                {
                    auto input = usable;
                    field(input)[lane] = word;
                    auto output = Dirty;
                    check(!TransformNodeWorldPointPcNearestForAnalysis(input.cache, input.point, output), "unqualified world point accepted");
                    check(output == Dirty, "refused operation modified output");
                }
        };
        badFields([](auto& input) -> auto& { return input.cache.positionBits; });
        badFields([](auto& input) -> auto& { return input.cache.scaleBits; });
        badFields([](auto& input) -> auto& { return input.cache.orientationBits; });
        badFields([](auto& input) -> auto& { return input.point; });
        auto cache = usable.cache;
        check(!TransformNodeWorldPointPcNearestForAnalysis(cache, usable.point, cache.positionBits), "position alias admitted");
        check(SameCache(cache, usable.cache), "refused position alias mutated cache");
        check(!TransformNodeWorldPointPcNearestForAnalysis(cache, usable.point, cache.scaleBits), "scale alias admitted");
        check(SameCache(cache, usable.cache), "refused scale alias mutated cache");
        std::cout << "NodeWorldPoint: " << checks << " checks passed\n";
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
