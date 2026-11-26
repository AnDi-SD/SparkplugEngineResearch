#include "Code/Sparkplug/spAnimationQuaternionSquadFactorPc.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis;

    bool Check(std::uint32_t input, std::uint32_t expected)
    {
        std::uint32_t actual = 0xDEADBEEFu;
        if (TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis(input, actual) && actual == expected)
            return true;
        std::cerr << "SQUAD factor mismatch for " << std::hex << input << '\n';
        return false;
    }

    int Replay()
    {
        std::string row;
        while (std::getline(std::cin, row))
        {
            std::uint32_t input = 0, output = 0;
            const auto parsed = std::from_chars(row.data(), row.data() + row.size(), input, 16);
            if (row.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != row.data() + row.size() ||
                !TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis(input, output))
                return 1;
            std::cout << std::hex << std::setfill('0') << std::setw(8) << output << '\n';
        }
        return std::cin.eof() && std::cout ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 2;

    bool passed = Check(0, 0);
    passed &= Check(0x80000000u, 0x80000000u);
    passed &= Check(0x3F800000u, 0);
    passed &= Check(0x3F000000u, 0x3F000000u);
    passed &= Check(0x3EAAAAABu, 0x3EE38E39u);
    passed &= Check(0x3E4CCCCDu, 0x3EA3D70Au);
    passed &= Check(0x3ECCCCCDu, 0x3EF5C28Fu);
    std::uint32_t unchanged = 0xDEADBEEFu;
    passed &= !TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis(0x3F800001u, unchanged) &&
              unchanged == 0xDEADBEEFu;
    if (!passed)
        return 1;

    std::cout << "AnimationQuaternionSquadFactorPc: 8 contracts passed\n";
    return 0;
}
