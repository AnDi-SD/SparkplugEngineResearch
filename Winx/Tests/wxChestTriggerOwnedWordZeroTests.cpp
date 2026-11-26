#include "Analysis/PC/wxChestTriggerAbi.h"
#include "Code/wxChestTrigger.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using Trigger = winx::reconstruction::wxChestTrigger;
    using Prefix = Trigger::CapturedPrefixForAnalysis;

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (8 * i));
    }

    Prefix Capture(std::uint32_t word, std::size_t salt)
    {
        Prefix state{};
        for (std::size_t i = 0; i < state.size(); ++i)
            state[i] = std::uint8_t(17 * salt + 37 * i + 5);
        StoreWord(state.data(), Abi::wxChestTriggerVTable);
        StoreWord(state.data() + Abi::wxChestTriggerOwnedWordOffset, word);
        return state;
    }

    std::uint32_t Evaluate(std::uint32_t word, std::size_t salt)
    {
        auto state = Capture(word, salt);
        const auto before = state;
        std::uint32_t eax = 0xA5FECDEFu;
        Require(Trigger::TryReadPcOwnedWordIsZeroForAnalysis(state, eax) && state == before,
            "qualified chest capture was refused or changed");
        return eax;
    }

    int Replay()
    {
        std::string line;
        std::size_t salt{};
        while (std::getline(std::cin, line))
        {
            std::uint32_t word{};
            const auto end = line.data() + line.size();
            const auto parsed = std::from_chars(line.data(), end, word, 16);
            if (line.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != end)
                throw std::runtime_error("expected an eight-digit hexadecimal field value");
            const auto eax = Evaluate(word, salt++);
            std::cout << std::hex << std::setfill('0') << std::setw(8) << eax << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::uint32_t field;
        std::uint32_t eax;
    };

    constexpr Witness Witnesses[]{
        {0x00000000u, 0x00000001u},
        {0x00000001u, 0x00000000u},
        {0x00000100u, 0x00000000u},
        {0x00010000u, 0x00000000u},
        {0x01000000u, 0x00000000u},
        {0x80000000u, 0x00000000u},
        {0xFFFFFFFFu, 0x00000000u},
        {0x80000001u, 0x00000000u},
        {0x7FFFFFFFu, 0x00000000u},
        {0xFFFFFF00u, 0x00000000u},
        {0x80010000u, 0x00000000u},
        {0x00008000u, 0x00000000u},
    };

    void CheckOwnedWord()
    {
        std::size_t salt{};
        for (const auto& witness : Witnesses)
            Require(Evaluate(witness.field, salt++) == witness.eax, "unexpected complete DWORD result");

        auto foreign = Capture(0u, 0);
        StoreWord(foreign.data(), 0x006FD630u);
        const auto before = foreign;
        std::uint32_t eax = 0xA5FECDEFu;
        Require(!Trigger::TryReadPcOwnedWordIsZeroForAnalysis(foreign, eax)
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
        CheckOwnedWord();
        std::cout << "ChestTriggerOwnedWordZero: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
