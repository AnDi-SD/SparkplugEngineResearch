#include "Code/Sparkplug/spBoundingVolumeSizePc.h"

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
    using sparkplug::reconstruction::ApplyBoundingVolumeSizePcNearestForAnalysis;
    using sparkplug::reconstruction::spBoundingVolumeSizePcForAnalysis;
    using Size = std::array<std::uint32_t, 3>;

    struct Witness final
    {
        std::string_view input;
        std::string_view output;
    };

    constexpr std::array<Witness, 18> Witnesses{{
        {"box 0000803f03776b39c4808939", "0000803f03776b39c48089390100003f"},
        {"box 0000803fbbcb6c392fee8839", "0000803fbbcb6c392fee88390100003f"},
        {"box 0000803f03776b39c3808939", "0000803f03776b39c38089390000003f"},
        {"box 000000800000000000000080", "00000080000000000000008000000000"},
        {"box 010000000000000000000000", "01000000000000000000000000000000"},
        {"box 010000000200008003000000", "01000000020000800300000002000000"},
        {"box ffff7f7fffff7fffffff7f7f", "ffff7f7fffff7fffffff7f7fd6b35d7f"},
        {"box cdcccc3dcdcccc3dcdcc8c3f", "cdcccc3dcdcccc3dcdcc8c3f79f50d3f"},
        {"box 00000040000080c000008040", "00000040000080c00000804000004040"},
        {"obb 0000803f03776b39c4808939", "0000803f03776b39c48089390100003f"},
        {"obb 0000803fbbcb6c392fee8839", "0000803fbbcb6c392fee88390100003f"},
        {"obb 0000803f03776b39c3808939", "0000803f03776b39c38089390000003f"},
        {"obb 000000800000000000000080", "00000080000000000000008000000000"},
        {"obb 010000000000000000000000", "01000000000000000000000000000000"},
        {"obb 010000000200008003000000", "01000000020000800300000002000000"},
        {"obb ffff7f7fffff7fffffff7f7f", "ffff7f7fffff7fffffff7f7fd6b35d7f"},
        {"obb 000000c000008040000080c0", "000000c000008040000080c000004040"},
        {"obb cdcccc3dcdcccc3dcdcc8c3f", "cdcccc3dcdcccc3dcdcc8c3f79f50d3f"},
    }};

    constexpr spBoundingVolumeSizePcForAnalysis Dirty{{0xF4E835CDu, 0x281B0A01u, 0x5C4F3E35u}, 0x7FA12345u};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Size Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        std::string role, text, extra;
        Require(bool(fields >> role >> text) && !(fields >> extra), "expected target kind and raw size");
        Require(role == "box" || role == "obb", "expected box or obb");
        Require(text.size() == 24, "incorrect size word count");
        Size result{};
        for (std::size_t lane = 0; lane < result.size(); ++lane)
            for (unsigned byte = 0; byte != 4; ++byte)
            {
                unsigned value = 0;
                const auto begin = text.data() + 8*lane + 2*byte;
                const auto parsed = std::from_chars(begin, begin + 2, value, 16);
                Require(parsed.ec == std::errc{} && parsed.ptr == begin + 2, "invalid hexadecimal byte");
                result[lane] |= std::uint32_t(value) << (8*byte);
            }
        return result;
    }

    std::string Encode(const spBoundingVolumeSizePcForAnalysis& target)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0');
        const std::array<std::uint32_t, 4> words{
            target.fullSizeBits[0], target.fullSizeBits[1], target.fullSizeBits[2], target.radiusBits};
        for (const auto word : words)
            for (unsigned byte = 0; byte != 4; ++byte)
                raw << std::setw(2) << static_cast<unsigned>(word >> (8*byte) & 0xFFu);
        return raw.str();
    }

    bool SameFields(const spBoundingVolumeSizePcForAnalysis& left, const spBoundingVolumeSizePcForAnalysis& right)
    {
        return left.fullSizeBits == right.fullSizeBits && left.radiusBits == right.radiusBits;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            const auto size = Parse(line);
            auto target = Dirty;
            Require(ApplyBoundingVolumeSizePcNearestForAnalysis(size, target), "qualified size refused");
            std::cout << Encode(target) << '\n';
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
            const auto size = Parse(row.input);
            const auto before = size;
            auto target = Dirty;
            check(ApplyBoundingVolumeSizePcNearestForAnalysis(size, target), "native size refused");
            check(Encode(target) == row.output, "size and radius differ from original");
            check(size == before, "operation changed the source size");
        }
        const auto usable = Parse(Witnesses[0].input);
        constexpr std::array<std::uint32_t, 6> Nonfinite{0x7F800000u, 0xFF800000u, 0x7FC12345u,
                                                          0xFFC12345u, 0x7FA12345u, 0xFFA12345u};
        for (const auto word : Nonfinite)
            for (std::size_t lane = 0; lane < usable.size(); ++lane)
            {
                auto size = usable;
                size[lane] = word;
                auto target = Dirty;
                check(!ApplyBoundingVolumeSizePcNearestForAnalysis(size, target), "nonfinite size accepted");
                check(SameFields(target, Dirty), "refused size changed the target");
            }
        auto target = Dirty;
        target.fullSizeBits = usable;
        const auto before = target;
        check(!ApplyBoundingVolumeSizePcNearestForAnalysis(target.fullSizeBits, target), "aliased source accepted");
        check(SameFields(target, before), "refused alias changed the target");
        std::cout << "BoundingVolumeSizePc: " << checks << " checks passed\n";
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
