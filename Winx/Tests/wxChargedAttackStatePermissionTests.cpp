#include "Code/wxChargedAttackState.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using State = winx::reconstruction::wxChargedAttackState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;
    using CharacterPrefix = State::CapturedCharacterPrefixForAnalysis;
    using Borrow = State::BorrowedCharacterForAnalysis;
    constexpr std::uint32_t CharacterAddress = 0x31000540;

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
            bytes[i] = std::uint8_t(value >> (8 * i));
    }

    StatePrefix CapturedState(std::uint32_t address)
    {
        StatePrefix state{};
        for (std::size_t i = 0; i < state.size(); ++i)
            state[i] = std::uint8_t(29 * i + 13);
        StoreWord(state.data(), 0x006F83F8);
        StoreWord(state.data() + 0x14, address);
        return state;
    }

    CharacterPrefix CapturedCharacter(std::uint32_t word)
    {
        CharacterPrefix character{};
        for (std::size_t i = 0; i < character.size(); ++i)
            character[i] = std::uint8_t(37 * i + 5);
        StoreWord(character.data(), 0x006F6438);
        StoreWord(character.data() + 0x140, word);
        return character;
    }

    std::uint8_t Evaluate(std::uint32_t target, std::uint32_t characterWord)
    {
        const auto state = CapturedState(target == 1u ? CharacterAddress : 0);
        const auto character = CapturedCharacter(characterWord);
        const auto before = state;
        const auto characterBefore = character;
        const Borrow borrowed = target == 1u ? Borrow{CharacterAddress, &character} : Borrow{};
        std::uint8_t output = 0xA5;
        Require(State::TryReadPcTransitionPermissionForAnalysis(state, target, borrowed, output)
            && state == before && character == characterBefore, "supported copied permission input was refused or changed");
        return output;
    }

    void CheckPermission()
    {
        struct Case
        {
            std::uint32_t target, characterWord;
            std::uint8_t al;
        };
        constexpr Case cases[]{
            {0x00000000u, 0x00000000u, 1},
            {0x0000001Bu, 0x00000000u, 0},
            {0x0000001Cu, 0x00000000u, 1},
            {0x0000001Du, 0x00000000u, 0},
            {0x0000001Eu, 0x00000000u, 1},
            {0x0000001Fu, 0x00000000u, 1},
            {0x00000020u, 0x00000000u, 0},
            {0x00000021u, 0x00000000u, 1},
            {0xFFFFFFFFu, 0x00000000u, 1},
            {0x00000101u, 0x00000000u, 1},
            {0x8000001Bu, 0x00000000u, 1},
            {0x80000001u, 0x00000000u, 1},
            {0x00000001u, 0x00000000u, 1},
            {0x00000001u, 0x00000003u, 0},
            {0x00000001u, 0xFFFFFFF3u, 0},
            {0x00000001u, 0x8000000Eu, 1},
        };
        for (const auto& fixture : cases)
            Require(Evaluate(fixture.target, fixture.characterWord) == fixture.al, "wrong transition permission AL");
    }

    void CheckAdmission()
    {
        const auto refuse = [](const StatePrefix& state, std::uint32_t target, const Borrow& borrow)
        {
            std::uint8_t output = 0xA5;
            Require(!State::TryReadPcTransitionPermissionForAnalysis(state, target, borrow, output)
                && output == 0xA5, "foreign or missing capture changed the output");
        };
        const auto state = CapturedState(CharacterAddress);
        const auto character = CapturedCharacter(3u);
        const Borrow borrow{CharacterAddress, &character};
        auto wrongState = state;
        wrongState[0] ^= 1;
        refuse(wrongState, 0u, {});
        refuse(state, 1u, {});
        refuse(state, 1u, {CharacterAddress, nullptr});
        refuse(state, 1u, {CharacterAddress + 4u, &character});
        refuse(CapturedState(0u), 1u, {0u, &character});
        auto wrongCharacter = character;
        wrongCharacter[0] ^= 1;
        refuse(state, 1u, {CharacterAddress, &wrongCharacter});

        std::uint8_t output = 0xA5;
        Require(State::TryReadPcTransitionPermissionForAnalysis(state, 2u, {0u, &wrongCharacter}, output)
            && output == 1, "unneeded character capture affected the direct mode decision");
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string target, characterWord, extra;
            if (!(fields >> target >> characterWord) || fields >> extra)
                throw std::runtime_error("expected mode and character word");
            const auto rawTarget = ParseWord(target);
            const auto rawCharacterWord = ParseWord(characterWord);
            if (rawTarget != 1u && rawCharacterWord != 0u)
                throw std::runtime_error("direct mode requires a zero character placeholder");
            const auto al = Evaluate(rawTarget, rawCharacterWord);
            std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(al) << '\n';
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
        std::cout << "ChargedAttackStatePermission: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
