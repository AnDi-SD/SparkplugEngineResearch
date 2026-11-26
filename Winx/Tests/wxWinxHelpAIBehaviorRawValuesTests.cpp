#include "Code/wxWinxHelpAIBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Target = wxWinxHelpAIBehavior;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Property = Target::PropertyForAnalysis;
    using Profile = Target::ProfileForAnalysis;
    using Composed = Target::ComposedRawPropertyForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryRawDwords1C4To1CC;

    struct Field
    {
        std::string_view name;
        Property property;
        std::size_t offset;
    };

    constexpr Field Fields[]{
        {"tecnaShieldTime", Property::TecnaShieldTime, 0x1C4},
        {"musaBlastDamage", Property::MusaBlastDamage, 0x1C8},
        {"stellaBlastDamage", Property::StellaBlastDamage, 0x1CC},
    };

    unsigned checks{};

    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
        ++checks;
    }

    const Field& FindField(std::string_view name)
    {
        for (const auto& field : Fields)
        {
            if (field.name == name)
                return field;
        }
        throw std::runtime_error("unknown Winx Help property selector");
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
            throw std::runtime_error("expected a 464-byte hexadecimal prefix");
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
        PutWord(prefix, 0, 0x006FFEE0);
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
            const auto& field = FindField(name);
            Composed result;
            Require(Target::ComposeRawPropertyForAnalysis(prefix, field.property, ParseHex(rawHex), result, Qualified),
                "qualified replay input was refused");
            std::cout << Hex(WordBytes(result.beforeRaw)) << ' ' << Hex(result.afterSet)
                << ' ' << Hex(WordBytes(result.afterRaw)) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRetainedProperties()
    {
        struct Case
        {
            std::array<std::uint32_t, 3> before;
            std::array<std::uint32_t, 3> arguments;
        };
        constexpr std::array<Case, 8> cases{{
            {{0x00003A98u, 0x0000000Fu, 0x0000000Fu}, {0x00003A98u, 0x0000000Fu, 0x0000000Fu}},
            {{0x00000000u, 0x00000000u, 0xFFFFFFFFu}, {0xFFFFFFFFu, 0x00000001u, 0x00000001u}},
            {{0x00000001u, 0xFFFFFFFEu, 0x7FFFFFFFu}, {0x00000000u, 0xFFFFFFFEu, 0x80000000u}},
            {{0xFFFFFFFFu, 0x7FFFFFFFu, 0x00000000u}, {0x00000000u, 0x80000000u, 0xFFFFFFFFu}},
            {{0x80000000u, 0x00000000u, 0x00000001u}, {0x80000001u, 0xFFFFFFFFu, 0x00000002u}},
            {{0x7FFFFFFFu, 0x80000000u, 0x7FC12345u}, {0x80000000u, 0x7FFFFFFFu, 0x7FC12345u}},
            {{0x7FC12345u, 0x12345678u, 0x80000000u}, {0x01020304u, 0x7FC12345u, 0x00000000u}},
            {{0xA5A5A5A5u, 0x55555555u, 0x12345678u}, {0xA5A5A5A5u, 0x00000000u, 0x01020304u}},
        }};
        for (unsigned i = 0; i < cases.size(); ++i)
        {
            auto state = Receiver(i);
            const auto& initial = cases[i].before;
            for (unsigned selector = 0; selector < initial.size(); ++selector)
                PutWord(state, Fields[selector].offset, initial[selector]);

            for (unsigned selector = 0; selector < initial.size(); ++selector)
            {
                const auto& field = Fields[selector];
                const auto arg = cases[i].arguments[selector];
                const auto before = state;
                auto expected = state;
                PutWord(expected, field.offset, arg);
                Composed composed;
                Require(Target::ComposeRawPropertyForAnalysis(state, field.property, arg, composed, Qualified),
                    "typed composed operation was refused");
                Require(composed.beforeRaw == initial[selector] && composed.afterRaw == arg,
                    "raw readback lost argument bits");
                Require(composed.afterSet == expected && state == before, "setter changed an unrelated byte");
                auto aliased = state;
                Require(Target::WriteRawPropertyForAnalysis(aliased, field.property, arg, aliased, Qualified)
                    && aliased == expected, "aliased raw setter differs from staged write");
                state = composed.afterSet;
            }
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
        const auto verify = [&](const Prefix& input, Property property, Profile profile)
        {
            Require(!Target::ReadRawPropertyForAnalysis(input, property, raw, profile) && raw == 0xABCD1234,
                "getter refusal changed output");
            Require(!Target::WriteRawPropertyForAnalysis(input, property, 0xFFFFFFFFu, written, profile)
                && written == sentinel, "setter refusal changed output");
            Require(!Target::ComposeRawPropertyForAnalysis(input, property, 0xFFFFFFFFu, composed, profile)
                && composed.beforeRaw == 0xDEADBEEF && composed.afterSet == sentinel && composed.afterRaw == 0xABCDEF01,
                "composed refusal changed output");
        };
        verify(before, Property::TecnaShieldTime, Profile::Unqualified);
        verify(before, Property::StellaBlastDamage, static_cast<Profile>(99));
        verify(before, static_cast<Property>(99), Qualified);
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FF490);
        verify(wrongPrimary, Property::TecnaShieldTime, Qualified);
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
        std::cout << "WinxHelpAIBehaviorRawValues: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
