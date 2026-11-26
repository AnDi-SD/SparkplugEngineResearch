#include "Code/Sparkplug/spSkyBoxManager.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <list>
#include <string_view>

using sparkplug::reconstruction::spSkyBoxManager;

int main(int argc, char** argv)
{
    const bool emitTrace = argc == 2 && std::string_view(argv[1]) == "--trace";
    constexpr std::uint32_t skyBoxClass = 0x7A7124AF;
    constexpr std::array addresses{1u, 2u, 1u, 3u, 1u, 3u};
    std::list<std::uint32_t> expected;
    spSkyBoxManager manager;
    if (!manager.GetSkyBoxesForAnalysis().empty()
        || manager.AppendSkyBoxForAnalysis({0, skyBoxClass})
        || manager.AppendSkyBoxForAnalysis({1, skyBoxClass + 1}))
    {
        std::cerr << "Unexpected empty or rejected list state\n";
        return 1;
    }

    for (const auto address : addresses)
    {
        expected.push_back(address);
        if (!manager.AppendSkyBoxForAnalysis({address, skyBoxClass})
            || manager.GetSkyBoxesForAnalysis() != expected)
        {
            std::cerr << "Borrowed list diverged after append\n";
            return 1;
        }
        if (emitTrace)
        {
            std::cout << address << ' ' << expected.size();
            for (const auto entry : manager.GetSkyBoxesForAnalysis())
                std::cout << ' ' << entry;
            std::cout << '\n';
        }
    }
    return 0;
}
