#include "Code/wxDroidAttackState.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using State = winx::reconstruction::wxDroidAttackState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;

    unsigned checks{};

    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseWord(std::string_view text)
    {
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, value, 16);
        if (text.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("expected an eight-digit hexadecimal word");
        return value;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (8 * i));
    }

    StatePrefix CapturedState(std::uint32_t fieldWord)
    {
        StatePrefix state{};
        for (std::size_t i = 0; i < state.size(); ++i)
            state[i] = static_cast<std::uint8_t>(29 * i + 13);
        StoreWord(state.data(), 0x006FA020);
        StoreWord(state.data() + 0x3C, fieldWord);
        return state;
    }

    std::uint32_t Evaluate(std::uint32_t target, std::uint32_t fieldWord)
    {
        const auto state = CapturedState(fieldWord);
        const auto before = state;
        std::uint32_t eax = 0xA5C37E19;
        Require(State::TryReadPcTransitionPermissionForAnalysis(state, target, eax) && state == before,
            "valid captured state was refused or changed");
        return eax;
    }

    void CheckPermission()
    {
        struct Case
        {
            std::uint32_t target, fieldWord, eax;
        };
        constexpr Case cases[]{
            {0x0000000Au, 0x00000000u, 1u},
            {0x0000000Au, 0xA5A500FFu, 1u},
            {0x00000000u, 0x00000000u, 0u},
            {0x00000000u, 0x01000000u, 0u},
            {0x00000000u, 0xFFFFFFFFu, 1u},
            {0x0000010Au, 0x00000100u, 0u},
            {0x8000000Au, 0x00000080u, 1u},
            {0xFFFFFFFFu, 0x00000002u, 1u},
            {0xFFFFFF0Au, 0x80000000u, 0u},
            {0x00000009u, 0x00000001u, 1u},
            {0x0000000Bu, 0x800000FFu, 1u},
            {0x00000001u, 0xFFFFFF00u, 0u},
        };
        for (const auto& fixture : cases)
            Require(Evaluate(fixture.target, fixture.fieldWord) == fixture.eax, "incorrect full EAX permission result");
    }

    void CheckAdmission()
    {
        auto state = CapturedState(0xFFFFFFFFu);
        StoreWord(state.data(), 0x006F83A8);
        const auto before = state;
        constexpr std::uint32_t targets[]{0u, 10u};
        for (const auto target : targets)
        {
            std::uint32_t eax = 0xA5C37E19;
            Require(!State::TryReadPcTransitionPermissionForAnalysis(state, target, eax)
                && eax == 0xA5C37E19 && state == before, "foreign captured state affected output or input");
        }
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string target, fieldWord, extra;
            if (!(fields >> target >> fieldWord) || fields >> extra)
                throw std::runtime_error("expected two hexadecimal words");
            const auto eax = Evaluate(ParseWord(target), ParseWord(fieldWord));
            std::cout << std::hex << std::setfill('0') << std::setw(8) << eax << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
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
        CheckPermission();
        CheckAdmission();
        std::cout << "DroidAttackStatePermission: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
