#include "Code/Sparkplug/spAnimationScalarCubicSamplePc.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::AnimationScalarCubicKeyPcForAnalysis;
    using sparkplug::reconstruction::TrySampleAnimationScalarCubicPcForAnalysis;
    using Key = AnimationScalarCubicKeyPcForAnalysis;

    std::uint32_t* Destination(std::string_view alias, Key& key, std::uint32_t& separate) noexcept
    {
        if (alias == "separate")
            return &separate;

        struct Alias
        {
            std::string_view name;
            std::uint32_t Key::*word;
        };
        constexpr std::array<Alias, 5> aliases{{
            {"value", &Key::valueBits},
            {"incoming", &Key::incomingBits},
            {"outgoing", &Key::outgoingBits},
            {"quadratic", &Key::quadraticBits},
            {"cubic", &Key::cubicBits},
        }};

        for (const auto& candidate : aliases)
        {
            if (candidate.name == alias)
                return &(key.*candidate.word);
        }
        return nullptr;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::size_t originalRecordIndex;
            std::string alias;
            std::uint32_t factor;
            Key key{};
            if (!(row >> originalRecordIndex >> alias >> std::hex >> factor >> key.valueBits >> key.incomingBits >>
                  key.outgoingBits >> key.quadraticBits >> key.cubicBits) || !(row >> std::ws).eof())
                return 1;

            // Each replay row contains the record selected by the original index.
            std::uint32_t separate = 0;
            auto* destination = Destination(alias, key, separate);
            if (!destination || !TrySampleAnimationScalarCubicPcForAnalysis(key, factor, *destination))
                return 1;
            std::cout << std::hex << std::setw(8) << std::setfill('0') << *destination << '\n';
        }
        return std::cin.eof() ? 0 : 1;
    }

    bool Check(std::string_view name, bool passed)
    {
        if (!passed)
            std::cerr << name << " failed\n";
        return passed;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return Replay();
    if (argc != 1)
        return 2;

    constexpr std::uint32_t Half = 0x3F000000u;
    constexpr Key Ordinary{0x40000000u, 0x7FC10203u, 0x40400000u, 0x40800000u, 0x40A00000u};
    auto output = 0xDEADBEEFu;
    bool passed = Check("ordinary polynomial", TrySampleAnimationScalarCubicPcForAnalysis(Ordinary, Half, output) &&
                                                   output == 0x40A40000u);

    constexpr Key Cancellation{0, 0xDEADBEEFu, 0xB3800000u, 0x3F800000u, 0x2F800000u};
    passed &= Check("extended cancellation", TrySampleAnimationScalarCubicPcForAnalysis(Cancellation, 0x33800000u, output) &&
                                               output == 0x0B800000u);

    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr Key Zeros{NegativeZero, 0, NegativeZero, NegativeZero, NegativeZero};
    passed &= Check("negative zero", TrySampleAnimationScalarCubicPcForAnalysis(Zeros, 0, output) &&
                                       output == NegativeZero);

    auto aliased = Ordinary;
    passed &= Check("quadratic output alias", TrySampleAnimationScalarCubicPcForAnalysis(aliased, Half, aliased.quadraticBits) &&
                                                aliased.quadraticBits == 0x40A40000u && aliased.cubicBits == Ordinary.cubicBits);

    output = 0xDEADBEEFu;
    passed &= Check("invalid interval factor", !TrySampleAnimationScalarCubicPcForAnalysis(Ordinary, 0xBF000000u, output) &&
                                                 output == 0xDEADBEEFu);
    auto invalid = Ordinary;
    invalid.outgoingBits = 0x7FA10203u;
    passed &= Check("invalid used record word", !TrySampleAnimationScalarCubicPcForAnalysis(invalid, Half, output) &&
                                                 output == 0xDEADBEEFu);

    if (!passed)
        return 1;
    std::cout << "AnimationScalarCubicSamplePc: 6 contracts passed\n";
    return 0;
}
