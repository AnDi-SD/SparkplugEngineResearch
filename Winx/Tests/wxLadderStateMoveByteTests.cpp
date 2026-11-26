#include "Analysis/PC/wxLadderStateAbi.h"
#include "Code/wxLadderState.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using State = winx::reconstruction::wxLadderState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;
    using CharacterPrefix = State::CapturedCharacterPrefixForAnalysis;
    using RecordPrefix = State::CapturedRecordPrefixForAnalysis;
    using Character = State::BorrowedCharacterForAnalysis;
    using Record = State::BorrowedRecordForAnalysis;

    constexpr std::uint32_t CharacterAddress = 0x31000540;
    constexpr std::uint32_t RecordAddress = 0x31000980;
    constexpr std::string_view InitialState =
        "08866f00000000000000cccc000000000e00000000000000000000000101010101cccccc0000"
        "00000000000000cccccc000000000000000000000000";
    constexpr std::string_view InitialCharacter =
        "38646f00000000000000cccc0000000000000000000000000000000000000000010000000000"
        "000000ccccccccccccccc0010031000000000101cccc4054094bf40100000000164300cccccc"
        "000000000000000000000000fc2f7000000000000000cccc0000000000000000000000000000"
        "0000000000000000000000000000000000000000000001ccccccffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f000000000000000000000000";

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    template<class UInt>
    UInt ParseUnsigned(std::string_view word)
    {
        UInt value{};
        const auto end = word.data() + word.size();
        const auto parsed = std::from_chars(word.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid hexadecimal capture");
        return value;
    }

    template<class Prefix>
    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        if (hex.size() != 2 * bytes.size())
            throw std::runtime_error("unexpected hexadecimal capture width");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = std::uint8_t(ParseUnsigned<unsigned>(hex.substr(i * 2, 2)));
        return bytes;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (i * 8));
    }

    template<class Prefix>
    void PrintPrefix(const Prefix& bytes)
    {
        for (const auto value : bytes)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string mode, stateHex, characterHex, recordHex, trailing;
            if (!(row >> mode >> stateHex >> characterHex >> recordHex) || (row >> trailing) || mode.size() != 8)
                throw std::runtime_error("expected mode and three captured prefixes");
            const auto rawTarget = ParseUnsigned<std::uint32_t>(mode);
            const auto state = ParsePrefix<StatePrefix>(stateHex);
            std::uint8_t al = 0xA5;
            if (rawTarget == 39u)
            {
                const auto character = ParsePrefix<CharacterPrefix>(characterHex);
                auto record = ParsePrefix<RecordPrefix>(recordHex);
                if (!State::TryApplyPcTransitionPermissionForAnalysis(state, rawTarget,
                        {CharacterAddress, &character}, {RecordAddress, &record}, al))
                    throw std::runtime_error("linked ladder captures were refused");
                std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(al) << ' ';
                PrintPrefix(state);
                std::cout << ' ';
                PrintPrefix(character);
                std::cout << ' ';
                PrintPrefix(record);
                std::cout << '\n';
            }
            else
            {
                if (characterHex != "-" || recordHex != "-"
                    || !State::TryApplyPcTransitionPermissionForAnalysis(state, rawTarget, {}, {}, al))
                    throw std::runtime_error("direct ladder replay requires only the state capture");
                std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(al) << ' ';
                PrintPrefix(state);
                std::cout << " - -\n";
            }
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::uint32_t mode;
        std::uint8_t oldByte;
        std::uint8_t al;
    };

    constexpr Witness Witnesses[]{
        {0x00000000u, 0x00, 1},
        {0x00000026u, 0x00, 1},
        {0x00000028u, 0x00, 1},
        {0x00000127u, 0x00, 1},
        {0x80000027u, 0x00, 1},
        {0xFFFFFFFFu, 0x00, 1},
        {0x00000027u, 0xA5, 0},
        {0x00000027u, 0x00, 0},
        {0x00000027u, 0xFF, 0},
        {0x00000027u, 0x01, 0},
        {0x00000027u, 0x80, 0},
        {0x00000027u, 0x00, 0},
    };

    void CheckWitnesses()
    {
        std::size_t index{};
        for (const auto& fixture : Witnesses)
        {
            auto state = ParsePrefix<StatePrefix>(InitialState);
            auto character = ParsePrefix<CharacterPrefix>(InitialCharacter);
            const bool linked = fixture.mode == 39u;
            StoreWord(state.data() + Abi::wxLadderStateCharacterLinkOffset, linked ? CharacterAddress : 0u);
            StoreWord(character.data() + Abi::wxLadderCharacterRecordLinkOffset, RecordAddress);
            const auto beforeState = state;
            const auto beforeCharacter = character;
            RecordPrefix record{};
            for (std::size_t i = 0; i < record.size(); ++i)
                record[i] = std::uint8_t(21 * index + 37 * i + 9);
            record[Abi::wxLadderRecordResetByteOffset] = fixture.oldByte;
            auto expected = record;
            if (linked)
                expected[Abi::wxLadderRecordResetByteOffset] = 0;
            const Character borrowed = linked ? Character{CharacterAddress, &character} : Character{};
            const Record destination = linked ? Record{RecordAddress, &record} : Record{};
            std::uint8_t al = 0xA5;
            Require(State::TryApplyPcTransitionPermissionForAnalysis(state, fixture.mode, borrowed, destination, al)
                && al == fixture.al, "unexpected ladder permission");
            Require(record == expected && state == beforeState && character == beforeCharacter, "unexpected capture effect");
            ++index;
        }
    }

    void CheckRejections()
    {
        auto state = ParsePrefix<StatePrefix>(InitialState);
        auto character = ParsePrefix<CharacterPrefix>(InitialCharacter);
        StoreWord(state.data() + Abi::wxLadderStateCharacterLinkOffset, CharacterAddress);
        StoreWord(character.data() + Abi::wxLadderCharacterRecordLinkOffset, RecordAddress);
        RecordPrefix record{};
        record.fill(0xA5);
        const auto prior = record;
        const Character source{CharacterAddress, &character};
        const Record destination{RecordAddress, &record};
        const auto refuse = [&](const StatePrefix& initial, const Character& borrower, const Record& output)
        {
            std::uint8_t al = 0xA5;
            Require(!State::TryApplyPcTransitionPermissionForAnalysis(initial, 39u, borrower, output, al)
                && al == 0xA5 && record == prior, "invalid linked ladder capture changed output");
        };
        auto wrongState = state;
        wrongState[0] ^= 1;
        refuse(wrongState, source, destination);
        wrongState = state;
        StoreWord(wrongState.data() + Abi::wxLadderStateCharacterLinkOffset, 0u);
        refuse(wrongState, {0u, &character}, destination);
        refuse(state, {CharacterAddress, nullptr}, destination);
        refuse(state, {CharacterAddress + 4u, &character}, destination);
        auto wrongCharacter = character;
        wrongCharacter[0] ^= 1;
        refuse(state, {CharacterAddress, &wrongCharacter}, destination);
        wrongCharacter = character;
        StoreWord(wrongCharacter.data() + Abi::wxLadderCharacterRecordLinkOffset, 0u);
        refuse(state, {CharacterAddress, &wrongCharacter}, {0u, &record});
        refuse(state, source, {RecordAddress, nullptr});
        refuse(state, source, {RecordAddress + 4u, &record});

        std::uint8_t al = 0xA5;
        Require(State::TryApplyPcTransitionPermissionForAnalysis(state, 0x80000027u,
            {0u, &wrongCharacter}, {0u, &record}, al) && al == 1u && record == prior,
            "direct mode consulted the linked captures");
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
        CheckWitnesses();
        CheckRejections();
        std::cout << "LadderStateMoveByte: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
