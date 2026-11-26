#include "Analysis/PC/wxWinxHelpStateMachineAbi.h"
#include "Code/wxWinxHelpStateMachine.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Machine = winx::reconstruction::wxWinxHelpStateMachine;
    using Prefix = Machine::CapturedPrefixForAnalysis;
    namespace Abi = winx::evidence::pc;

    constexpr std::string_view InitialPrefix =
        "30b46f00000000000000cccc0000000000000000000000000000000000000000010000000000"
        "000000cccccccccccccc90020031000000000101cccc4054094bf40100000000164300cccccc"
        "000000000000000000000000fc2f7000000000000000cccc0000000000000000000000000000"
        "0000000000000000000000000000000000000000000001ccccccffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f000000000100000000000000"
        "0000000000000000010000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        "cccccccc000000000000000001cccccccccccccc";

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t Hex(std::string_view digits)
    {
        std::uint32_t value{};
        const auto end = digits.data() + digits.size();
        const auto parsed = std::from_chars(digits.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid hexadecimal capture");
        return value;
    }

    Prefix Capture(std::uint32_t raw)
    {
        Prefix state{};
        static_assert(InitialPrefix.size() == 2 * Prefix{}.size());
        for (std::size_t i = 0; i < state.size(); ++i)
            state[i] = std::uint8_t(Hex(InitialPrefix.substr(2 * i, 2)));
        for (std::size_t i = 0; i < 4; ++i)
            state[Abi::wxWinxHelpStateMachineRawCodeFieldOffset + i] = std::uint8_t(raw >> (8 * i));
        return state;
    }

    std::uint32_t Evaluate(std::uint32_t raw, std::uint32_t argument)
    {
        auto state = Capture(raw);
        const auto before = state;
        std::uint32_t eax = 0xA5FECDEFu;
        Require(Machine::TryReadPcMaskedCodeForAnalysis(state, argument, eax) && state == before,
            "qualified machine capture was refused or changed");
        return eax;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string field, argument, extra;
            if (!(row >> field >> argument) || (row >> extra) || field.size() != 8 || argument.size() != 8)
                throw std::runtime_error("expected two eight-digit hexadecimal words");
            const auto eax = Evaluate(Hex(field), Hex(argument));
            std::cout << std::hex << std::setfill('0') << std::setw(8) << eax << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::uint32_t field;
        std::uint32_t argument;
        std::uint32_t eax;
    };

    constexpr Witness Witnesses[]{
        {0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000080u, 0x00000000u, 0x00000016u},
        {0x00000080u, 0xFFFFFFFFu, 0x00000016u},
        {0x00000080u, 0x12345678u, 0x00000016u},
        {0x0000007Fu, 0x00000002u, 0x00000000u},
        {0x000000FFu, 0x00000080u, 0x00000016u},
        {0x00000100u, 0xA5A5A5A5u, 0x00000000u},
        {0x00000180u, 0x00000001u, 0x00000000u},
        {0x00004080u, 0xFFFFFFFFu, 0x00000000u},
        {0x00007F80u, 0x00000000u, 0x00000000u},
        {0x00008000u, 0x80000000u, 0x00000000u},
        {0x00008080u, 0xA55AA55Au, 0x00000016u},
        {0x80000080u, 0xFFFFFFFFu, 0x00000016u},
        {0x80018085u, 0x7FFFFFFFu, 0x00000016u},
        {0xFFFF80FFu, 0xDEADBEEFu, 0x00000016u},
        {0xFFFFFFFFu, 0x0000001Bu, 0x00000000u},
    };

    void CheckMaskedCode()
    {
        for (const auto& witness : Witnesses)
            Require(Evaluate(witness.field, witness.argument) == witness.eax, "unexpected full DWORD result");

        auto foreign = Capture(0x80u);
        foreign[0] ^= 1u;
        const auto before = foreign;
        std::uint32_t eax = 0xA5FECDEFu;
        Require(!Machine::TryReadPcMaskedCodeForAnalysis(foreign, 0xFFFFFFFFu, eax)
            && eax == 0xA5FECDEFu && foreign == before, "foreign capture changed state or result");
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
        CheckMaskedCode();
        std::cout << "WinxHelpStateMachineMaskedCode: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
