#include "Code/wxSpiderAIBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Behavior = wxSpiderAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    using Composed = Behavior::ComposedIsSpiderForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryIsSpiderRawByte1B0;
    constexpr std::size_t IsSpiderOffset = 0x1B0;

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
            throw std::runtime_error("expected a 433-byte hexadecimal prefix");
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
        PutWord(prefix, 0, 0x006FF840);
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
            Require(Behavior::ComposeIsSpiderLowByteForAnalysis(prefix, ParseHex(rawHex), result, Qualified),
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
            prefix[IsSpiderOffset] = std::uint8_t(value ^ 0xA5u);
            const auto before = prefix;
            auto expected = before;
            expected[IsSpiderOffset] = std::uint8_t(value);
            const auto argument = (0xA5C30000u ^ (value << 8)) | value;
            Composed composed;
            Require(Behavior::ComposeIsSpiderLowByteForAnalysis(prefix, argument, composed, Qualified),
                "typed composed operation was refused");
            Require(composed.beforeRaw == before[IsSpiderOffset] && composed.afterRaw == std::uint8_t(value),
                "raw byte readback lost argument bits");
            Require(composed.afterSet == expected && prefix == before, "setter changed an unrelated byte");
            Require(Behavior::WriteIsSpiderLowByteForAnalysis(prefix, argument, prefix, Qualified)
                && prefix == expected, "aliased byte setter differs from staged write");
        }

        auto prefix = Receiver();
        prefix[IsSpiderOffset] = 0x80;
        Composed composed;
        Require(Behavior::ComposeIsSpiderLowByteForAnalysis(prefix, 0x12FEDC80, composed, Qualified)
            && composed.beforeRaw == 0x80 && composed.afterSet == prefix && composed.afterRaw == 0x80,
            "equal low-byte write did not preserve the complete prefix");
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
            Require(!Behavior::ReadIsSpiderByteForAnalysis(input, raw, profile) && raw == 0x5C,
                "getter refusal changed output");
            Require(!Behavior::WriteIsSpiderLowByteForAnalysis(input, 0xFFFFFFFFu, written, profile)
                && written == sentinel, "setter refusal changed output");
            Require(!Behavior::ComposeIsSpiderLowByteForAnalysis(input, 0xFFFFFFFFu, composed, profile)
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
        CheckRefusals();
        std::cout << "SpiderAIBehaviorIsSpiderByte: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
