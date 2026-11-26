#include "Analysis/PC/wxSparxBehaviorAbi.h"
#include "Code/wxSparxBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Behavior = wxSparxBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::TypeProfileForAnalysis;
    using Composed = Behavior::ComposedSparxTypeRawForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimarySparxTypeRawDword124;
    constexpr std::size_t TypeOffset = 0x124;
    static_assert(wxSparxBehaviorAbi::SparxTypeRawDwordOffset == TypeOffset);
    static_assert(wxSparxBehaviorAbi::SparxTypeGetterPC == 0x00555050);
    static_assert(wxSparxBehaviorAbi::SparxTypeSetterPC == 0x00555040);

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
        if (text.size() != 2 * prefix.size() || text.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("expected a 297-byte hexadecimal prefix");
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
            prefix[byte] = std::uint8_t(salt * 13 + 0x3B + byte * 37);
        PutWord(prefix, 0, 0x006F6F60);
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string prefixHex, rawHex, extra;
            if (!(fields >> prefixHex >> rawHex) || fields >> extra || rawHex.size() != 8
                || rawHex.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("expected a captured prefix and eight argument digits");
            const auto prefix = ParsePrefix(prefixHex);
            Composed result;
            Require(Behavior::ComposeSparxTypeRawForAnalysis(prefix, ParseHex(rawHex), result, Qualified),
                "qualified replay input was refused");
            std::cout << Hex(WordBytes(result.beforeRaw)) << ' ' << Hex(result.afterSet)
                << ' ' << Hex(WordBytes(result.afterRaw)) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRawValues()
    {
        constexpr std::array<std::uint32_t, 8> seeds{
            0, 1, 2, 0xFFFFFFFF, 0x80000001, 0x12345678, 0x01000000, 0xA5A55A5A,
        };
        constexpr std::array<std::uint32_t, 8> arguments{
            0, 0xFFFFFFFF, 0x3F100000, 1, 0xDEADBEEF, 0x12345678, 0x80000000, 0x55AA33CC,
        };
        for (unsigned i = 0; i < seeds.size(); ++i)
        {
            auto prefix = Receiver(i);
            PutWord(prefix, TypeOffset, seeds[i]);
            const auto before = prefix;
            auto expected = before;
            PutWord(expected, TypeOffset, arguments[i]);
            Composed composed;
            Require(Behavior::ComposeSparxTypeRawForAnalysis(prefix, arguments[i], composed, Qualified),
                "typed composed operation was refused");
            Require(composed.beforeRaw == seeds[i] && composed.afterRaw == arguments[i],
                "raw readback lost argument bits");
            Require(composed.afterSet == expected && prefix == before, "setter changed an unrelated byte");
            Require(Behavior::WriteSparxTypeRawForAnalysis(prefix, arguments[i], prefix, Qualified)
                && prefix == expected, "aliased raw setter differs from staged write");
            std::uint8_t testAnimation{};
            Require(Behavior::ReadTestAnimationByteForAnalysis(prefix, testAnimation,
                Behavior::ProfileForAnalysis::PcCapturedPrimaryTestAnimationRawByte128)
                && testAnimation == before[0x128], "Sparx Type write changed the separate Test Animation read");
        }
    }

    void CheckRefusals()
    {
        const auto before = Receiver();
        Prefix written;
        written.fill(0xC7);
        const auto sentinel = written;
        std::uint32_t raw = 0xABCD1234;
        Composed composed{0xDEADBEEF, sentinel, 0xABCDEF01};
        const auto verify = [&](const Prefix& input, Profile profile)
        {
            Require(!Behavior::ReadSparxTypeRawForAnalysis(input, raw, profile) && raw == 0xABCD1234,
                "getter refusal changed output");
            Require(!Behavior::WriteSparxTypeRawForAnalysis(input, 0xFFFFFFFFu, written, profile)
                && written == sentinel, "setter refusal changed output");
            Require(!Behavior::ComposeSparxTypeRawForAnalysis(input, 0xFFFFFFFFu, composed, profile)
                && composed.beforeRaw == 0xDEADBEEF && composed.afterSet == sentinel && composed.afterRaw == 0xABCDEF01,
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
        std::cout << "SparxBehaviorTypeRawValue: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
