#include "Analysis/PC/wxGolemAttackStateAbi.h"
#include "Code/wxGolemAttackState.h"

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
    using State = winx::reconstruction::wxGolemAttackState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;
    using CharacterPrefix = State::CapturedCharacterPrefixForAnalysis;
    using RecordPrefix = State::CapturedRecordPrefixForAnalysis;
    using Character = State::BorrowedCharacterForAnalysis;
    using Record = State::BorrowedRecordForAnalysis;

    constexpr std::uint32_t CharacterAddress = 0x31000540;
    constexpr std::uint32_t RecordAddress = 0x31000980;
    constexpr std::string_view InitialState =
        "20926f00000000000000cccc000000000300000040050031000000000101010101cccccc0000"
        "00000000000000cccccc00000000000000000000000000000000";
    constexpr std::string_view InitialCharacter =
        "38646f00000000000000cccc0000000000000000000000000000000000000000010000000000"
        "000000ccccccccccccccc0010031000000000101cccc4054094bf40100000000164300cccccc"
        "000000000000000000000000fc2f7000000000000000cccc0000000000000000000000000000"
        "0000000000000000000000000000000000000000000001ccccccffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f000000000000000080090031";

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
        if (hex.size() != 2 * bytes.size() || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
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
            std::string stateHex, characterHex, recordHex, trailing;
            if (!(row >> stateHex >> characterHex >> recordHex) || row >> trailing)
                throw std::runtime_error("expected state, character and record captures");
            const auto state = ParsePrefix<StatePrefix>(stateHex);
            const auto character = ParsePrefix<CharacterPrefix>(characterHex);
            auto record = ParsePrefix<RecordPrefix>(recordHex);
            if (!State::TryClearPcBorrowedRecordWordForAnalysis(state,
                    {CharacterAddress, &character}, {RecordAddress, &record}))
                throw std::runtime_error("linked Golem state captures were refused");
            PrintPrefix(state);
            std::cout << ' ';
            PrintPrefix(character);
            std::cout << ' ';
            PrintPrefix(record);
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    void CheckWitnesses()
    {
        constexpr std::uint32_t words[]{
            0x00000000u, 0xffffffffu, 0x80000000u, 0x00000001u,
            0x12345678u, 0x00000000u, 0x00523690u, 0xaa55aa55u,
        };
        const auto state = ParsePrefix<StatePrefix>(InitialState);
        const auto character = ParsePrefix<CharacterPrefix>(InitialCharacter);
        const auto beforeState = state;
        const auto beforeCharacter = character;
        std::size_t index = 0;
        for (const auto word : words)
        {
            RecordPrefix record{};
            for (std::size_t byte = 0; byte < record.size(); ++byte)
                record[byte] = std::uint8_t(17 * index + 37 * byte + 9);
            StoreWord(record.data() + Abi::wxGolemAttackStateRecordClearWordOffset, word);
            auto expected = record;
            StoreWord(expected.data() + Abi::wxGolemAttackStateRecordClearWordOffset, 0);
            Require(State::TryClearPcBorrowedRecordWordForAnalysis(state,
                        {CharacterAddress, &character}, {RecordAddress, &record}),
                "valid Golem state capture was refused");
            Require(record == expected && state == beforeState && character == beforeCharacter,
                "unexpected Golem state capture effect");
            ++index;
        }
    }

    void CheckRejections()
    {
        const auto state = ParsePrefix<StatePrefix>(InitialState);
        const auto character = ParsePrefix<CharacterPrefix>(InitialCharacter);
        RecordPrefix record{};
        record.fill(0xA5);
        const auto prior = record;
        const Character source{CharacterAddress, &character};
        const Record destination{RecordAddress, &record};
        const auto refuse = [&](const StatePrefix& initial, const Character& borrower, const Record& output)
        {
            Require(!State::TryClearPcBorrowedRecordWordForAnalysis(initial, borrower, output)
                && record == prior, "invalid Golem state capture changed the record");
        };
        auto wrongState = state;
        wrongState[0] ^= 1;
        refuse(wrongState, source, destination);
        wrongState = state;
        StoreWord(wrongState.data() + Abi::wxGolemAttackStateCharacterLinkOffset, 0);
        refuse(wrongState, {0, &character}, destination);
        refuse(state, {CharacterAddress, nullptr}, destination);
        refuse(state, {CharacterAddress + 4, &character}, destination);

        auto wrongCharacter = character;
        wrongCharacter[0] ^= 1;
        refuse(state, {CharacterAddress, &wrongCharacter}, destination);
        wrongCharacter = character;
        StoreWord(wrongCharacter.data() + Abi::wxGolemAttackStateCharacterRecordLinkOffset, 0);
        refuse(state, {CharacterAddress, &wrongCharacter}, {0, &record});
        refuse(state, source, {RecordAddress, nullptr});
        refuse(state, source, {RecordAddress + 4, &record});
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
        std::cout << "GolemAttackStateRecordClear: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
