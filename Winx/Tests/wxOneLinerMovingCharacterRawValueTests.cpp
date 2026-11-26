#include "../Code/wxOneLinerMovingCharacter.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using Character = wxOneLinerMovingCharacter;
using Prefix = Character::CapturedPrefixForAnalysis;
using Profile = Character::ProfileForAnalysis;
constexpr auto PcProfile = Profile::PcCapturedPrimaryOneLinerIdRawDword124;
constexpr std::uint32_t Sentinel = 0x5CD75CD7;

void PutWord(Prefix& bytes, std::size_t offset, std::uint32_t word)
{
    for (unsigned byte = 0; byte < 4; ++byte)
    {
        bytes[offset + byte] = static_cast<std::uint8_t>(word >> (byte * 8));
    }
}

Prefix ParsePrefix(const std::string& token)
{
    Prefix prefix{};
    if (token.size() != prefix.size() * 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
    {
        throw std::runtime_error("expected a 296-byte hexadecimal prefix");
    }
    for (std::size_t byte = 0; byte < prefix.size(); ++byte)
    {
        prefix[byte] = static_cast<std::uint8_t>(std::stoul(token.substr(byte * 2, 2), nullptr, 16));
    }
    return prefix;
}

std::uint32_t ParseWord(const std::string& token)
{
    if (token.size() != 8 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
    {
        throw std::runtime_error("expected a four-byte hexadecimal word");
    }
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
    {
        value |= static_cast<std::uint32_t>(std::stoul(token.substr(byte * 2, 2), nullptr, 16)) << (byte * 8);
    }
    return value;
}

void PrintBytes(const std::uint8_t* bytes, std::size_t size)
{
    std::cout << std::hex << std::setfill('0');
    for (std::size_t byte = 0; byte < size; ++byte)
    {
        std::cout << std::setw(2) << unsigned(bytes[byte]);
    }
    std::cout << '\n';
}

int Replay(bool store)
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream input(line);
        std::string token;
        std::string word;
        std::string extra;
        if (!(input >> token) || (store && !(input >> word)) || input >> extra)
        {
            throw std::runtime_error(store ? "expected a prefix and value per line" : "expected one prefix per line");
        }
        auto prefix = ParsePrefix(token);
        if (store)
        {
            if (!Character::WriteOneLinerIdRawForAnalysis(prefix, ParseWord(word), PcProfile))
            {
                throw std::runtime_error("PC receiver prefix was refused");
            }
            PrintBytes(prefix.data(), prefix.size());
            continue;
        }
        std::uint32_t output = Sentinel;
        if (!Character::ReadOneLinerIdRawForAnalysis(prefix, output, PcProfile))
        {
            throw std::runtime_error("PC receiver prefix was refused");
        }
        std::array<std::uint8_t, 4> bytes{};
        for (unsigned byte = 0; byte < bytes.size(); ++byte)
        {
            bytes[byte] = static_cast<std::uint8_t>(output >> (byte * 8));
        }
        PrintBytes(bytes.data(), bytes.size());
    }
    if (!std::cin.eof() || !std::cout)
    {
        throw std::runtime_error("replay input or output failed");
    }
    return 0;
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay")
        {
            return Replay(false);
        }
        if (argc == 2 && std::string(argv[1]) == "--replay-set")
        {
            return Replay(true);
        }
        if (argc != 1)
        {
            return 2;
        }

        unsigned checks = 0;
        auto require = [&](bool value, const char* message)
        {
            if (!value)
            {
                throw std::runtime_error(message);
            }
            ++checks;
        };
        constexpr std::uint32_t words[]{0u, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu,
            0x7F800000u, 0xFF800000u, 0x7FC00001u, 0x80000001u, 1u, 0x01020304u,
            0xA5A5A5A5u, 0x5A5A5A5Au, 0x12345678u, 0xFEDCBA98u, 0x00010000u, 0xDEADBEEFu};
        for (const auto word : words)
        {
            for (unsigned salt = 0; salt < 4; ++salt)
            {
                Prefix prefix{};
                for (std::size_t byte = 0; byte < prefix.size(); ++byte)
                {
                    prefix[byte] = static_cast<std::uint8_t>(salt * 17 + byte * 37);
                }
                PutWord(prefix, 0, 0x006FE124);
                PutWord(prefix, 0x124, word);
                const auto before = prefix;
                std::uint32_t output = Sentinel;
                require(Character::ReadOneLinerIdRawForAnalysis(prefix, output, PcProfile), "valid prefix was refused");
                require(output == word, "raw DWORD changed");
                require(prefix == before, "valid prefix changed");
            }

            Prefix prefix{};
            prefix.fill(0xA7);
            PutWord(prefix, 0, 0x006FE124);
            PutWord(prefix, 0x124, ~word);
            auto expected = prefix;
            PutWord(expected, 0x124, word);
            require(Character::WriteOneLinerIdRawForAnalysis(prefix, word, PcProfile), "valid prefix was refused on write");
            require(prefix == expected, "write bytes or remainder of prefix differ");
            std::uint32_t output = Sentinel;
            require(Character::ReadOneLinerIdRawForAnalysis(prefix, output, PcProfile), "written prefix was refused on readback");
            require(output == word, "written raw DWORD changed on readback");
            require(Character::WriteOneLinerIdRawForAnalysis(prefix, word, PcProfile), "idempotent write was refused");
            require(prefix == expected, "idempotent write changed bytes");
        }

        Prefix valid{};
        PutWord(valid, 0, 0x006FE124);
        const auto before = valid;
        std::uint32_t output = Sentinel;
        require(!Character::ReadOneLinerIdRawForAnalysis(valid, output), "default profile was accepted");
        require(output == Sentinel, "default profile changed output");
        require(valid == before, "default profile changed prefix");
        require(!Character::ReadOneLinerIdRawForAnalysis(valid, output, static_cast<Profile>(99)), "unknown profile was accepted");
        require(output == Sentinel, "unknown profile changed output");
        require(valid == before, "unknown profile changed prefix");
        require(!Character::WriteOneLinerIdRawForAnalysis(valid, Sentinel), "default profile was accepted on write");
        require(valid == before, "refused default write changed prefix");
        require(!Character::WriteOneLinerIdRawForAnalysis(valid, Sentinel, static_cast<Profile>(99)), "unknown profile was accepted on write");
        require(valid == before, "refused unknown write changed prefix");
        for (unsigned byte = 0; byte < 4; ++byte)
        {
            auto wrong = valid;
            wrong[byte] ^= 1;
            const auto wrongBefore = wrong;
            require(!Character::ReadOneLinerIdRawForAnalysis(wrong, output, PcProfile), "wrong primary was accepted");
            require(output == Sentinel, "wrong primary changed output");
            require(wrong == wrongBefore, "wrong primary changed prefix");
            require(!Character::WriteOneLinerIdRawForAnalysis(wrong, Sentinel, PcProfile), "wrong primary was accepted on write");
            require(wrong == wrongBefore, "refused wrong-primary write changed prefix");
        }
        std::cout << "OneLinerMovingCharacterRawValue: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
