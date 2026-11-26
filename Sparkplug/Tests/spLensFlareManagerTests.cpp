#include "Code/Sparkplug/spLensFlareManager.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

using sparkplug::reconstruction::spLensFlareManager;

namespace
{
    using Flare = spLensFlareManager::CapturedFlareForAnalysis;
    using Scene = std::array<std::uint32_t, 14>;

    Scene ReadScene(const spLensFlareManager& first, const spLensFlareManager& second,
                    const std::array<const Flare*, 4>& flares)
    {
        Scene state{first.GetFirstFlareForAnalysis(), first.GetLastFlareForAnalysis(),
                    first.GetFlareCountForAnalysis(), second.GetFirstFlareForAnalysis(),
                    second.GetLastFlareForAnalysis(), second.GetFlareCountForAnalysis()};
        std::size_t offset = 6;
        for (auto* flare : flares)
        {
            state[offset++] = flare->GetPreviousForAnalysis();
            state[offset++] = flare->GetNextForAnalysis();
        }
        return state;
    }
}

int main(int argc, char** argv)
{
    const bool emitTrace = argc == 2 && std::string_view(argv[1]) == "--trace";
    constexpr std::uint32_t flareClass = 0x435370B5;
    constexpr std::uint32_t a = 1;
    constexpr std::uint32_t b = 2;
    constexpr std::uint32_t c = 3;
    constexpr std::uint32_t d = 4;
    Flare itemA(a, flareClass);
    Flare itemB(b, flareClass);
    Flare itemC(c, flareClass);
    Flare itemD(d, flareClass);
    spLensFlareManager first;
    spLensFlareManager second;
    const std::array<const Flare*, 4> flares{&itemA, &itemB, &itemC, &itemD};

    bool passed = true;
    std::size_t ordinal = 0;
    const auto step = [&](bool admitted, const Scene& expected)
    {
        const auto current = ReadScene(first, second, flares);
        if (!admitted || current != expected)
        {
            std::cerr << "Unexpected membership at row " << ordinal << '\n';
            passed = false;
        }
        if (emitTrace)
        {
            std::cout << ordinal;
            for (auto value : current)
                std::cout << ' ' << value;
            std::cout << '\n';
        }
        ++ordinal;
    };

    step(true, {});
    step(first.AppendFlareForAnalysis(itemA), {a, a, 1});
    const auto beforeGuards = ReadScene(first, second, flares);
    Flare duplicateAddress(a, flareClass);
    Flare wrongClass(9, flareClass + 1);
    Flare nullAddress(0, flareClass);
    const bool duplicate = first.AppendFlareForAnalysis(itemA);
    const bool aliased = first.AppendFlareForAnalysis(duplicateAddress);
    const bool wrongOwnerAppend = second.AppendFlareForAnalysis(itemA);
    const bool wrongOwnerUnlink = second.UnlinkFlareForAnalysis(itemA);
    const bool detachedUnlink = first.UnlinkFlareForAnalysis(itemB);
    const bool wrongClassAppend = first.AppendFlareForAnalysis(wrongClass);
    const bool nullAppend = first.AppendFlareForAnalysis(nullAddress);
    if (duplicate || aliased || wrongOwnerAppend || wrongOwnerUnlink || detachedUnlink
        || wrongClassAppend || nullAppend || ReadScene(first, second, flares) != beforeGuards)
    {
        std::cerr << "Portable admission guard changed a captured list\n";
        passed = false;
    }

    step(first.AppendFlareForAnalysis(itemB), {a, b, 2, 0, 0, 0, 0, b, a, 0});
    step(first.AppendFlareForAnalysis(itemC), {a, c, 3, 0, 0, 0, 0, b, a, c, b, 0});
    step(second.AppendFlareForAnalysis(itemD), {a, c, 3, d, d, 1, 0, b, a, c, b, 0});
    step(first.UnlinkFlareForAnalysis(itemB), {a, c, 2, d, d, 1, 0, c, 0, 0, a, 0});
    step(second.AppendFlareForAnalysis(itemB), {a, c, 2, d, b, 2, 0, c, d, 0, a, 0, 0, b});
    step(first.UnlinkFlareForAnalysis(itemA), {c, c, 1, d, b, 2, 0, 0, d, 0, 0, 0, 0, b});
    step(second.UnlinkFlareForAnalysis(itemB), {c, c, 1, d, d, 1});
    step(first.UnlinkFlareForAnalysis(itemC), {0, 0, 0, d, d, 1});
    step(second.UnlinkFlareForAnalysis(itemD), {});
    return passed ? 0 : 1;
}
