#include "Code/wxMinotaurAIBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Target = wxMinotaurAIBehavior;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Profile = Target::ProfileForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryIdDwordAndCt2Byte;

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
            throw std::runtime_error("expected a 442-byte hexadecimal prefix");
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
        PutWord(prefix, 0, 0x006FFC48);
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string name, prefixHex, rawHex, extra;
            if (!(fields >> name >> prefixHex >> rawHex) || fields >> extra || rawHex.size() != 8)
                throw std::runtime_error("expected a selector, captured prefix and eight argument digits");
            const auto prefix = ParsePrefix(prefixHex);
            const auto argument = ParseHex(rawHex);
            if (name == "id")
            {
                Target::ComposedIdForAnalysis result;
                Require(Target::ComposeRawIdForAnalysis(prefix, argument, result, Qualified),
                    "qualified ID replay input was refused");
                std::cout << Hex(WordBytes(result.beforeRaw)) << ' ' << Hex(result.afterSet)
                    << ' ' << Hex(WordBytes(result.afterRaw)) << '\n';
            }
            else if (name == "ct2")
            {
                Target::ComposedCt2ForAnalysis result;
                Require(Target::ComposeCt2LowByteForAnalysis(prefix, argument, result, Qualified),
                    "qualified Ct2 replay input was refused");
                std::cout << Hex(std::array{result.beforeRaw}) << ' ' << Hex(result.afterSet)
                    << ' ' << Hex(std::array{result.afterRaw}) << '\n';
            }
            else
            {
                throw std::runtime_error("unknown Minotaur property selector");
            }
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRetainedProperties()
    {
        struct Case
        {
            std::uint32_t idBefore;
            std::uint32_t idArgument;
            std::uint8_t ct2Before;
            std::uint32_t ct2Argument;
            std::uint32_t energy;
            std::uint8_t byte1B8;
        };
        constexpr std::array<Case, 8> cases{{
            {0x00000001u, 0x00000001u, 0x00, 0xABCD1200u, 0x3E99999Au, 0x00},
            {0x00000000u, 0xFFFFFFFFu, 0x01, 0x00000002u, 0x3F800000u, 0xAB},
            {0xFFFFFFFFu, 0x00000000u, 0x02, 0xFEDCBA01u, 0x80000000u, 0x34},
            {0x80000000u, 0x80000000u, 0x7F, 0x12345680u, 0x7FC00001u, 0xFF},
            {0x7FFFFFFFu, 0x80000001u, 0x80, 0x00ABCDFFu, 0x00000000u, 0x12},
            {0xA5A5A5A5u, 0x01020304u, 0xFF, 0xFFFFFE00u, 0x11223344u, 0x80},
            {0x7FC12345u, 0x7FC00001u, 0x55, 0x01020355u, 0xFFC00000u, 0xA5},
            {0x01020304u, 0xA5A5A5A5u, 0xAA, 0x80FF017Fu, 0x5AA55AA5u, 0x55},
        }};
        for (unsigned i = 0; i < cases.size(); ++i)
        {
            const auto& fixture = cases[i];
            auto prefix = Receiver(i);
            PutWord(prefix, 0x1B0, fixture.idBefore);
            PutWord(prefix, 0x1B4, fixture.energy);
            prefix[0x1B8] = fixture.byte1B8;
            prefix[0x1B9] = fixture.ct2Before;

            const auto beforeId = prefix;
            auto expectedId = beforeId;
            PutWord(expectedId, 0x1B0, fixture.idArgument);
            Target::ComposedIdForAnalysis id;
            Require(Target::ComposeRawIdForAnalysis(prefix, fixture.idArgument, id, Qualified),
                "qualified ID operation was refused");
            Require(id.beforeRaw == fixture.idBefore && id.afterRaw == fixture.idArgument,
                "ID readback lost DWORD bits");
            Require(id.afterSet == expectedId && prefix == beforeId, "ID write changed an unrelated byte");
            auto aliasedId = prefix;
            Require(Target::WriteRawIdForAnalysis(aliasedId, fixture.idArgument, aliasedId, Qualified)
                && aliasedId == expectedId, "aliased ID setter differs from staged write");

            const auto beforeCt2 = id.afterSet;
            auto expectedCt2 = beforeCt2;
            expectedCt2[0x1B9] = std::uint8_t(fixture.ct2Argument);
            Target::ComposedCt2ForAnalysis ct2;
            Require(Target::ComposeCt2LowByteForAnalysis(id.afterSet, fixture.ct2Argument, ct2, Qualified),
                "qualified Ct2 operation was refused");
            Require(ct2.beforeRaw == fixture.ct2Before && ct2.afterRaw == expectedCt2[0x1B9],
                "Ct2 readback differs from the low argument byte");
            Require(ct2.afterSet == expectedCt2 && id.afterSet == beforeCt2, "Ct2 write changed an unrelated byte");
            auto aliasedCt2 = id.afterSet;
            Require(Target::WriteCt2LowByteForAnalysis(aliasedCt2, fixture.ct2Argument, aliasedCt2, Qualified)
                && aliasedCt2 == expectedCt2, "aliased Ct2 setter differs from staged write");
        }
    }

    void CheckRefusals()
    {
        const auto before = Receiver();
        Prefix written;
        written.fill(0xC7);
        const auto sentinel = written;
        std::uint32_t rawId = 0xABCD1234;
        std::uint8_t rawCt2 = 0xF7;
        Target::ComposedIdForAnalysis id{0xDEADBEEF, sentinel, 0xABCDEF01};
        Target::ComposedCt2ForAnalysis ct2{0xBA, sentinel, 0xAB};
        const auto verify = [&](const Prefix& input, Profile profile)
        {
            Require(!Target::ReadRawIdForAnalysis(input, rawId, profile) && rawId == 0xABCD1234,
                "ID getter refusal changed output");
            Require(!Target::WriteRawIdForAnalysis(input, 0u, written, profile) && written == sentinel,
                "ID setter refusal changed output");
            Require(!Target::ComposeRawIdForAnalysis(input, 0u, id, profile)
                && id.beforeRaw == 0xDEADBEEF && id.afterSet == sentinel && id.afterRaw == 0xABCDEF01,
                "composed ID refusal changed output");
            Require(!Target::ReadRawCt2ByteForAnalysis(input, rawCt2, profile) && rawCt2 == 0xF7,
                "Ct2 getter refusal changed output");
            Require(!Target::WriteCt2LowByteForAnalysis(input, 0u, written, profile) && written == sentinel,
                "Ct2 setter refusal changed output");
            Require(!Target::ComposeCt2LowByteForAnalysis(input, 0u, ct2, profile)
                && ct2.beforeRaw == 0xBA && ct2.afterSet == sentinel && ct2.afterRaw == 0xAB,
                "composed Ct2 refusal changed output");
        };
        verify(before, Profile::Unqualified);
        verify(before, static_cast<Profile>(99));
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FF490);
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
        CheckRetainedProperties();
        CheckRefusals();
        std::cout << "MinotaurAIBehaviorRawValues: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
