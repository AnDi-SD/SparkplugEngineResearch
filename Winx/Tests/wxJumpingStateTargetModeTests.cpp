#include "Analysis/PC/wxGhoulJumpingStateAbi.h"
#include "Analysis/PC/wxJumpingStateAbi.h"
#include "Code/wxGhoulJumpingState.h"
#include "Code/wxJumpingState.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using winx::reconstruction::wxGhoulJumpingState;
    using winx::reconstruction::wxJumpingState;

    static_assert(winx::evidence::pc::wxJumpingStateVTable != winx::evidence::pc::GhoulJumpingTable);
    static_assert(winx::evidence::pc::wxJumpingStateTargetMode == winx::evidence::pc::GhoulJumpingHook38);

    struct Case
    {
        std::uint32_t rawTarget;
        std::uint32_t eax;
    };

    constexpr Case Cases[]{
        {0x00000000u, 0x00000002u},
        {0x00000001u, 0x00000001u},
        {0x00000002u, 0x00000001u},
        {0x00000003u, 0x00000002u},
        {0x00000004u, 0x00000001u},
        {0x00000012u, 0x00000001u},
        {0x00000100u, 0x00000001u},
        {0x00000103u, 0x00000001u},
        {0x01000003u, 0x00000001u},
        {0x80000000u, 0x00000001u},
        {0x80000003u, 0x00000001u},
        {0xFFFFFFFFu, 0x00000001u},
    };

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::uint32_t argument{};
            const auto end = line.data() + line.size();
            const auto parsed = std::from_chars(line.data(), end, argument, 16);
            if (line.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != end)
                throw std::runtime_error("expected exactly eight hexadecimal argument digits");

            const auto eax = wxJumpingState::EvaluateTargetModeForAnalysis(argument);
            std::cout << std::hex << std::setfill('0') << std::setw(8) << eax << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        if (argc != 1)
            return 2;

        const wxGhoulJumpingState ghoul;
        unsigned checked = 0;
        for (const auto& test : Cases)
        {
            if (wxJumpingState::EvaluateTargetModeForAnalysis(test.rawTarget) != test.eax)
                throw std::runtime_error("wxJumpingState PC slot +0x38 returned an unexpected DWORD");
            ++checked;
            if (ghoul.vfunc_38(test.rawTarget) != test.eax)
                throw std::runtime_error("wxGhoulJumpingState PC slot +0x38 returned an unexpected DWORD");
            ++checked;
        }
        std::cout << "JumpingStateTargetMode: " << checked << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
