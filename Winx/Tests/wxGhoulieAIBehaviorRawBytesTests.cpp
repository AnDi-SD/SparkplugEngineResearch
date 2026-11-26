#include "Code/wxGhoulieAIBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Behavior = wxGhoulieAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    using Composed = Behavior::ComposedRawByteForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryRawBytes1C4And1C5;
    using Property = Behavior::PropertyForAnalysis;
    constexpr std::size_t GardeniaOffset = 0x1C4;
    constexpr std::size_t JumpingOffset = 0x1C5;

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
            throw std::runtime_error("expected a 454-byte hexadecimal prefix");
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
        PutWord(prefix, 0, 0x006FF338);
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string selector, prefixHex, rawHex, extra;
            if (!(fields >> selector >> prefixHex >> rawHex) || fields >> extra || rawHex.size() != 8)
                throw std::runtime_error("expected a selector, captured prefix and eight argument digits");
            Property property;
            if (selector == "gardenia")
                property = Property::IsInGardenia01;
            else if (selector == "jumping")
                property = Property::JumpingGhoulie;
            else
                throw std::runtime_error("unknown Ghoulie property selector");
            const auto prefix = ParsePrefix(prefixHex);
            Composed result;
            Require(Behavior::ComposeLowByteForAnalysis(prefix, property, ParseHex(rawHex), result, Qualified),
                "qualified replay input was refused");
            std::cout << Hex(std::array<std::uint8_t, 1>{result.beforeRaw}) << ' ' << Hex(result.afterSet)
                << ' ' << Hex(std::array<std::uint8_t, 1>{result.afterRaw}) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRawValues(Property property, std::size_t offset)
    {
        for (unsigned value = 0; value < 256; ++value)
        {
            auto prefix = Receiver(value);
            prefix[offset] = std::uint8_t(value ^ 0xA5u);
            const auto before = prefix;
            auto expected = before;
            expected[offset] = std::uint8_t(value);
            const auto argument = (0xA5C30000u ^ (value << 8)) | value;
            Composed composed;
            Require(Behavior::ComposeLowByteForAnalysis(prefix, property, argument, composed, Qualified),
                "typed composed operation was refused");
            Require(composed.beforeRaw == before[offset] && composed.afterRaw == std::uint8_t(value),
                "raw byte readback lost argument bits");
            Require(composed.afterSet == expected && prefix == before, "setter changed an unrelated byte");
            Require(Behavior::WriteLowByteForAnalysis(prefix, property, argument, prefix, Qualified)
                && prefix == expected, "aliased byte setter differs from staged write");
        }

        auto prefix = Receiver();
        prefix[offset] = 0x80;
        Composed composed;
        Require(Behavior::ComposeLowByteForAnalysis(prefix, property, 0x12FEDC80, composed, Qualified)
            && composed.beforeRaw == 0x80 && composed.afterSet == prefix && composed.afterRaw == 0x80,
            "equal low-byte write did not preserve the complete prefix");
    }

    void CheckRetainedBytes()
    {
        auto expected = Receiver();
        Composed gardenia, jumping;
        expected[GardeniaOffset] = 0x02;
        Require(Behavior::ComposeLowByteForAnalysis(
            Receiver(), Property::IsInGardenia01, 0xABCD1202, gardenia, Qualified),
            "Gardenia retained operation was refused");
        Require(gardenia.afterSet == expected, "Gardenia store altered the Jumping byte or unrelated state");
        expected[JumpingOffset] = 0xFF;
        Require(Behavior::ComposeLowByteForAnalysis(
            gardenia.afterSet, Property::JumpingGhoulie, 0x123456FF, jumping, Qualified)
            && jumping.afterSet == expected, "Jumping store failed to retain the Gardenia write");
    }

    void CheckRefusals()
    {
        const auto before = Receiver();
        Prefix written;
        written.fill(0xC7);
        const auto sentinel = written;
        std::uint8_t raw = 0x5C;
        Composed composed{0x6E, sentinel, 0x19};
        const auto verify = [&](const Prefix& input, Property property, Profile profile)
        {
            Require(!Behavior::ReadRawByteForAnalysis(input, property, raw, profile) && raw == 0x5C,
                "getter refusal changed output");
            Require(!Behavior::WriteLowByteForAnalysis(input, property, 0xFFFFFFFFu, written, profile)
                && written == sentinel, "setter refusal changed output");
            Require(!Behavior::ComposeLowByteForAnalysis(input, property, 0xFFFFFFFFu, composed, profile)
                && composed.beforeRaw == 0x6E && composed.afterSet == sentinel && composed.afterRaw == 0x19,
                "composed refusal changed output");
        };
        verify(before, Property::IsInGardenia01, Profile::Unqualified);
        verify(before, Property::JumpingGhoulie, static_cast<Profile>(99));
        verify(before, static_cast<Property>(99), Qualified);
        auto wrongPrimary = before;
        wrongPrimary[0] ^= 1;
        verify(wrongPrimary, Property::IsInGardenia01, Qualified);
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
        CheckRawValues(Property::IsInGardenia01, GardeniaOffset);
        CheckRawValues(Property::JumpingGhoulie, JumpingOffset);
        CheckRetainedBytes();
        CheckRefusals();
        std::cout << "GhoulieAIBehaviorRawBytes: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
