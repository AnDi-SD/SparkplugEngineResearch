#include "Code/wxStudentsStateMachine.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Machine = wxStudentsStateMachine;
    using Prefix = Machine::CapturedPrefixForAnalysis;
    using Profile = Machine::ProfileForAnalysis;
    using Composed = Machine::ComposedTrollFightForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryTrollFightRawByte284;
    constexpr std::size_t TrollFightOffset = 0x284;

    unsigned checks{};

    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseHex(std::string_view text)
    {
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("expected hexadecimal input");
        return value;
    }

    Prefix ParsePrefix(std::string_view text)
    {
        Prefix prefix{};
        if (text.size() != 2 * prefix.size())
            throw std::runtime_error("expected a 645-byte hexadecimal prefix");
        for (std::size_t byte = 0; byte < prefix.size(); ++byte)
            prefix[byte] = std::uint8_t(ParseHex(text.substr(2 * byte, 2)));
        return prefix;
    }

    template<std::size_t N>
    std::string Hex(const std::array<std::uint8_t, N>& bytes)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(2 * bytes.size());
        for (const auto byte : bytes)
        {
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }
        return result;
    }

    std::array<std::uint8_t, 4> WordBytes(std::uint32_t word)
    {
        return {std::uint8_t(word), std::uint8_t(word >> 8), std::uint8_t(word >> 16), std::uint8_t(word >> 24)};
    }

    void PutWord(Prefix& prefix, std::size_t offset, std::uint32_t word)
    {
        for (const auto byte : WordBytes(word))
            prefix[offset++] = byte;
    }

    Prefix Receiver(unsigned salt = 0)
    {
        Prefix prefix{};
        for (std::size_t byte = 0; byte < prefix.size(); ++byte)
            prefix[byte] = std::uint8_t(salt * 17 + 5 + byte * 37);
        PutWord(prefix, 0, 0x006FB280);
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string prefixHex, rawHex, extra;
            if (!(fields >> prefixHex >> rawHex) || fields >> extra || rawHex.size() != 8)
                throw std::runtime_error("expected a captured prefix and eight argument digits");
            const auto prefix = ParsePrefix(prefixHex);
            Composed result;
            Require(Machine::ComposeTrollFightLowByteForAnalysis(prefix, ParseHex(rawHex), result, Qualified),
                "qualified replay input was refused");
            std::cout << Hex(std::array<std::uint8_t, 1>{result.beforeRaw}) << ' ' << Hex(result.afterSet)
                << ' ' << Hex(std::array<std::uint8_t, 1>{result.afterRaw}) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRawValues()
    {
        for (unsigned value = 0; value < 256; ++value)
        {
            auto prefix = Receiver(value);
            prefix[TrollFightOffset] = std::uint8_t(value ^ 0xA5u);
            const auto before = prefix;
            auto expected = before;
            expected[TrollFightOffset] = std::uint8_t(value);
            const auto argument = (0xA5C30000u ^ (value << 8)) | value;
            Composed composed;
            Require(Machine::ComposeTrollFightLowByteForAnalysis(prefix, argument, composed, Qualified),
                "typed composed operation was refused");
            Require(composed.beforeRaw == before[TrollFightOffset] && composed.afterRaw == std::uint8_t(value),
                "raw byte readback lost argument bits");
            Require(composed.afterSet == expected && prefix == before, "setter changed an unrelated byte");
            Require(Machine::WriteTrollFightLowByteForAnalysis(prefix, argument, prefix, Qualified)
                && prefix == expected, "aliased byte setter differs from staged write");
        }

        auto prefix = Receiver();
        prefix[TrollFightOffset] = 0x80;
        Composed composed;
        Require(Machine::ComposeTrollFightLowByteForAnalysis(prefix, 0x12FEDC80, composed, Qualified)
            && composed.beforeRaw == 0x80 && composed.afterSet == prefix && composed.afterRaw == 0x80,
            "equal low-byte write did not preserve the complete prefix");
    }

    void CheckRetainedFixtures()
    {
        struct Fixture
        {
            std::uint8_t prior;
            std::uint32_t argument;
        };
        constexpr Fixture fixtures[]{
            {0x00, 0xABCD1200u},
            {0x01, 0x00000002u},
            {0x02, 0xFEDCBA01u},
            {0x7F, 0x12345680u},
            {0x80, 0x00ABCDFFu},
            {0xFF, 0xFFFFFE00u},
            {0x55, 0x01020355u},
            {0xAA, 0x80FF017Fu},
        };
        unsigned salt = 0;
        for (const auto& fixture : fixtures)
        {
            auto prefix = Receiver(salt++);
            prefix[TrollFightOffset] = fixture.prior;
            auto expected = prefix;
            expected[TrollFightOffset] = std::uint8_t(fixture.argument);
            Composed observed;
            Require(Machine::ComposeTrollFightLowByteForAnalysis(prefix, fixture.argument, observed, Qualified)
                && observed.beforeRaw == fixture.prior && observed.afterRaw == expected[TrollFightOffset]
                && observed.afterSet == expected, "retained Troll Fight bytes differ");
        }
    }

    void CheckRefusals()
    {
        const auto before = Receiver();
        Prefix written;
        written.fill(0xC7);
        const auto sentinel = written;
        std::uint8_t raw = 0x5C;
        Composed composed{0x6E, sentinel, 0x19};
        const auto verify = [&](const Prefix& input, Profile profile)
        {
            Require(!Machine::ReadTrollFightByteForAnalysis(input, raw, profile) && raw == 0x5C,
                "getter refusal changed output");
            Require(!Machine::WriteTrollFightLowByteForAnalysis(input, 0xFFFFFFFFu, written, profile)
                && written == sentinel, "setter refusal changed output");
            Require(!Machine::ComposeTrollFightLowByteForAnalysis(input, 0xFFFFFFFFu, composed, profile)
                && composed.beforeRaw == 0x6E && composed.afterSet == sentinel && composed.afterRaw == 0x19,
                "composed refusal changed output");
        };
        verify(before, Profile::Unqualified);
        verify(before, static_cast<Profile>(99));
        auto wrongPrimary = before;
        wrongPrimary[0] ^= 1;
        verify(wrongPrimary, Qualified);
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
        CheckRawValues();
        CheckRetainedFixtures();
        CheckRefusals();
        std::cout << "StudentsStateMachineTrollFightByte: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
