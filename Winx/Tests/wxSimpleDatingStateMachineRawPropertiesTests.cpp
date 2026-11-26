#include "Code/wxSimpleDatingStateMachine.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Target = wxSimpleDatingStateMachine;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Profile = Target::ProfileForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryBloomByteAndDateDword;

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
            throw std::runtime_error("expected a 564-byte hexadecimal prefix");
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
        PutWord(prefix, 0, 0x006FB340);
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
            if (name == "date")
            {
                Target::ComposedDateForAnalysis result;
                Require(Target::ComposeRawDateForAnalysis(prefix, argument, result, Qualified),
                    "qualified Date Number replay input was refused");
                std::cout << Hex(WordBytes(result.beforeRaw)) << ' ' << Hex(result.afterSet)
                    << ' ' << Hex(WordBytes(result.afterRaw)) << '\n';
            }
            else if (name == "bloom")
            {
                Target::ComposedBloomForAnalysis result;
                Require(Target::ComposeBloomLowByteForAnalysis(prefix, argument, result, Qualified),
                    "qualified Bloom replay input was refused");
                std::cout << Hex(std::array{result.beforeRaw}) << ' ' << Hex(result.afterSet)
                    << ' ' << Hex(std::array{result.afterRaw}) << '\n';
            }
            else
            {
                throw std::runtime_error("unknown Simple Dating property selector");
            }
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRetainedProperties()
    {
        struct Case
        {
            std::uint8_t bloomBefore;
            std::uint32_t bloomArgument;
            std::uint32_t dateBefore;
            std::uint32_t dateArgument;
        };
        constexpr std::array<Case, 8> cases{{
            {0x01, 0xABCD1201u, 0x00000001u, 0x00000001u},
            {0x00, 0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu},
            {0x02, 0xFEDCBA01u, 0xFFFFFFFFu, 0x00000000u},
            {0x7F, 0x12345680u, 0x80000000u, 0x80000001u},
            {0x80, 0x00ABCDFFu, 0x7FFFFFFFu, 0x80000000u},
            {0xFF, 0xFFFFFE00u, 0xA5A5A5A5u, 0x01020304u},
            {0x55, 0x01020355u, 0x7FC12345u, 0x7FC12345u},
            {0xAA, 0x80FF017Fu, 0x01020304u, 0xA5A5A5A5u},
        }};
        for (unsigned i = 0; i < cases.size(); ++i)
        {
            const auto& fixture = cases[i];
            auto prefix = Receiver(i);
            prefix[0x22C] = fixture.bloomBefore;
            PutWord(prefix, 0x230, fixture.dateBefore);

            const auto beforeBloom = prefix;
            auto expectedBloom = beforeBloom;
            expectedBloom[0x22C] = std::uint8_t(fixture.bloomArgument);
            Target::ComposedBloomForAnalysis bloom;
            Require(Target::ComposeBloomLowByteForAnalysis(prefix, fixture.bloomArgument, bloom, Qualified),
                "qualified Bloom operation was refused");
            Require(bloom.beforeRaw == fixture.bloomBefore && bloom.afterRaw == expectedBloom[0x22C],
                "Bloom readback differs from the low argument byte");
            Require(bloom.afterSet == expectedBloom && prefix == beforeBloom, "Bloom store changed an unrelated byte");
            auto aliasedBloom = prefix;
            Require(Target::WriteBloomLowByteForAnalysis(aliasedBloom, fixture.bloomArgument, aliasedBloom, Qualified)
                && aliasedBloom == expectedBloom, "aliased Bloom setter differs from staged write");

            const auto beforeDate = bloom.afterSet;
            auto expectedDate = beforeDate;
            PutWord(expectedDate, 0x230, fixture.dateArgument);
            Target::ComposedDateForAnalysis date;
            Require(Target::ComposeRawDateForAnalysis(bloom.afterSet, fixture.dateArgument, date, Qualified),
                "qualified Date Number operation was refused");
            Require(date.beforeRaw == fixture.dateBefore && date.afterRaw == fixture.dateArgument,
                "Date Number readback lost DWORD bits");
            Require(date.afterSet == expectedDate && bloom.afterSet == beforeDate, "Date Number store changed an unrelated byte");
            auto aliasedDate = bloom.afterSet;
            Require(Target::WriteRawDateForAnalysis(aliasedDate, fixture.dateArgument, aliasedDate, Qualified)
                && aliasedDate == expectedDate, "aliased Date Number setter differs from staged write");
        }
    }

    void CheckRefusals()
    {
        const auto before = Receiver();
        Prefix written;
        written.fill(0xC7);
        const auto sentinel = written;
        std::uint32_t rawDate = 0xABCD1234;
        std::uint8_t rawBloom = 0xF7;
        Target::ComposedDateForAnalysis date{0xDEADBEEF, sentinel, 0xABCDEF01};
        Target::ComposedBloomForAnalysis bloom{0xBA, sentinel, 0xAB};
        const auto verify = [&](const Prefix& input, Profile profile)
        {
            Require(!Target::ReadRawDateForAnalysis(input, rawDate, profile) && rawDate == 0xABCD1234,
                "Date Number getter refusal changed output");
            Require(!Target::WriteRawDateForAnalysis(input, 0u, written, profile) && written == sentinel,
                "Date Number setter refusal changed output");
            Require(!Target::ComposeRawDateForAnalysis(input, 0u, date, profile)
                && date.beforeRaw == 0xDEADBEEF && date.afterSet == sentinel && date.afterRaw == 0xABCDEF01,
                "composed Date Number refusal changed output");
            Require(!Target::ReadRawBloomForAnalysis(input, rawBloom, profile) && rawBloom == 0xF7,
                "Bloom getter refusal changed output");
            Require(!Target::WriteBloomLowByteForAnalysis(input, 0u, written, profile) && written == sentinel,
                "Bloom setter refusal changed output");
            Require(!Target::ComposeBloomLowByteForAnalysis(input, 0u, bloom, profile)
                && bloom.beforeRaw == 0xBA && bloom.afterSet == sentinel && bloom.afterRaw == 0xAB,
                "composed Bloom refusal changed output");
        };
        verify(before, Profile::Unqualified);
        verify(before, static_cast<Profile>(99));
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FAA58);
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
        std::cout << "SimpleDatingStateMachineRawProperties: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
