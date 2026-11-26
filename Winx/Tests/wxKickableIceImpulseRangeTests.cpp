#include "Analysis/PC/wxKickableIceAbi.h"
#include "Code/wxKickableIce.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Target = wxKickableIce;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Returned = Target::ImpulseRangeReturnForAnalysis;
    using Profile = Target::ImpulseRangeProfileForAnalysis;
    using Result = Target::ImpulseRangeResultForAnalysis;
    using Composed = Target::ComposedImpulseRangeForAnalysis;
    namespace Abi = wxKickableIceAbi;
    constexpr auto Qualified = Profile::PcCapturedPrimaryImpulseRangeF32;
    constexpr auto RawQualified = Target::ProfileForAnalysis::PcCapturedPrimaryIceRawDwords;
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

    template<std::size_t N>
    std::array<std::uint8_t, N> ParseBytes(std::string_view text)
    {
        std::array<std::uint8_t, N> result{};
        if (text.size() != 2 * result.size())
            throw std::runtime_error("wrong hexadecimal byte extent");
        for (std::size_t byte = 0; byte < result.size(); ++byte)
            result[byte] = std::uint8_t(ParseHex(text.substr(2 * byte, 2)));
        return result;
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

    void PutWord(Prefix& prefix, std::size_t offset, std::uint32_t word)
    {
        for (unsigned byte = 0; byte < 4; ++byte)
            prefix[offset + byte] = std::uint8_t(word >> (8 * byte));
    }

    Prefix Receiver(unsigned salt, std::uint32_t rangeBits)
    {
        Prefix prefix{};
        for (std::size_t byte = 0; byte < prefix.size(); ++byte)
            prefix[byte] = std::uint8_t(salt * 17 + 5 + byte * 37);
        PutWord(prefix, 0, Abi::CapturedPrimary);
        PutWord(prefix, Abi::ImpulseRangeOffset, rangeBits);
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
            const auto prefix = ParseBytes<Prefix{}.size()>(prefixHex);
            Composed result;
            Require(Target::ComposeImpulseRangeForAnalysis(prefix, ParseHex(rawHex), result, Qualified)
                == Result::Completed, "qualified replay input was refused");
            std::cout << Hex(result.before) << ' ' << Hex(result.afterSet) << ' ' << Hex(result.after) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckQualified()
    {
        struct Case
        {
            std::uint32_t before, argument, id, type, stars;
            std::string_view beforeST0, afterST0;
        };
        constexpr Case cases[]{
            {0x3F800000u, 0x3F800000u, 0x00000000u, 0x00000002u, 0x00000000u, "0000000000000080ff3f", "0000000000000080ff3f"},
            {0x40000000u, 0x3DCCCCCDu, 0xFFFFFFFFu, 0x80000000u, 0x00000028u, "00000000000000800040", "0000000000cdccccfb3f"},
            {0x00800000u, 0x7F7FFFFFu, 0x12345678u, 0x00000001u, 0xFFFFFF00u, "0000000000000080813f", "0000000000ffffff7e40"},
            {0x7F7FFFFFu, 0x00800000u, 0x00000000u, 0xA5A5A5A5u, 0x00001234u, "0000000000ffffff7e40", "0000000000000080813f"},
            {0x3F800001u, 0x3F800001u, 0x12345678u, 0xFFFFFFFFu, 0x01020304u, "0000000000010080ff3f", "0000000000010080ff3f"},
            {0x3F000001u, 0x41200000u, 0x80000001u, 0x7FFFFFFFu, 0x00000001u, "0000000000010080fe3f", "00000000000000a00240"},
            {0x00000001u, 0x007FFFFFu, 0x01020304u, 0x7FC12345u, 0x80000100u, "00000000000000806a3f", "0000000000feffff803f"},
            {0x007FFFFFu, 0x00800000u, 0x55555555u, 0xDEADBEEFu, 0x00000000u, "0000000000feffff803f", "0000000000000080813f"},
        };
        unsigned salt = 0;
        for (const auto& trial : cases)
        {
            auto before = Receiver(salt++, trial.before);
            PutWord(before, Abi::RawOffsets[0], trial.id);
            PutWord(before, Abi::RawOffsets[1], trial.type);
            PutWord(before, Abi::RawOffsets[2], trial.stars);
            auto state = before;
            auto expected = before;
            PutWord(expected, Abi::ImpulseRangeOffset, trial.argument);
            Composed observed;
            Require(Target::ComposeImpulseRangeForAnalysis(state, trial.argument, observed, Qualified)
                == Result::Completed, "qualified impulse range refused");
            Require(observed.before == ParseBytes<10>(trial.beforeST0)
                && observed.after == ParseBytes<10>(trial.afterST0), "wrong physical x87 value");
            Require(state == before && observed.afterSet == expected, "range store changed unrelated bytes");
            Require(Target::WriteImpulseRangeBitsForAnalysis(state, trial.argument, state, Qualified)
                == Result::Completed && state == expected, "aliased range store differs");
        }
    }

    void CheckRefusals()
    {
        Prefix prefixSentinel;
        Returned oldSentinel, newSentinel;
        prefixSentinel.fill(0xC7);
        oldSentinel.fill(0x9D);
        newSentinel.fill(0xE6);
        const auto unchanged = [&](const Composed& value)
        {
            return value.before == oldSentinel && value.afterSet == prefixSentinel && value.after == newSentinel;
        };
        const auto verify = [&](const Prefix& before, Profile profile, Result wanted)
        {
            auto raw = oldSentinel;
            auto written = prefixSentinel;
            Composed composed{oldSentinel, prefixSentinel, newSentinel};
            Require(Target::ReadImpulseRangeForAnalysis(before, raw, profile) == wanted && raw == oldSentinel,
                "getter refusal changed output or status");
            Require(Target::WriteImpulseRangeBitsForAnalysis(before, 0x3F800000, written, profile) == wanted
                && written == prefixSentinel, "setter refusal changed output or status");
            Require(Target::ComposeImpulseRangeForAnalysis(before, 0x3F800000, composed, profile) == wanted
                && unchanged(composed), "composed refusal changed output or status");
        };

        const auto before = Receiver(0, 0x3F800000);
        verify(before, Profile::Unqualified, Result::UnqualifiedProfile);
        verify(before, static_cast<Profile>(99), Result::UnqualifiedProfile);
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FC930);
        verify(wrongPrimary, Qualified, Result::CapturedPrimaryNotAdmitted);

        constexpr std::uint32_t rejected[]{
            0u, 0x80000000u, 0xBF800000u, 0x80000001u, 0x7F800000u, 0xFF800000u, 0x7FC12345u, 0x7F800001u,
        };
        for (const auto bits : rejected)
        {
            auto invalidCapture = before;
            PutWord(invalidCapture, Abi::ImpulseRangeOffset, bits);
            auto raw = oldSentinel;
            auto written = prefixSentinel;
            Composed composed{oldSentinel, prefixSentinel, newSentinel};
            Require(Target::ReadImpulseRangeForAnalysis(invalidCapture, raw, Qualified) == Result::CapturedRangeNotAdmitted
                && raw == oldSentinel, "reader admitted an out-of-domain range");
            Require(Target::ComposeImpulseRangeForAnalysis(invalidCapture, 0x3F800000, composed, Qualified)
                == Result::CapturedRangeNotAdmitted && unchanged(composed), "composed reader admitted an out-of-domain range");
            Require(Target::WriteImpulseRangeBitsForAnalysis(before, bits, written, Qualified)
                == Result::ArgumentNotAdmitted && written == prefixSentinel, "setter admitted an out-of-domain argument");
            Require(Target::ComposeImpulseRangeForAnalysis(before, bits, composed, Qualified)
                == Result::ArgumentNotAdmitted && unchanged(composed), "composed setter admitted an out-of-domain argument");

            for (const auto property : {Target::PropertyForAnalysis::IceId, Target::PropertyForAnalysis::CollectibleType,
                    Target::PropertyForAnalysis::FairyStarsValue})
            {
                auto expected = invalidCapture;
                PutWord(expected, Abi::RawOffsets[static_cast<unsigned>(property)], 0x11223344u);
                Target::ComposedRawPropertyForAnalysis other;
                Require(Target::ComposeRawPropertyForAnalysis(invalidCapture, property, 0x11223344u, other, RawQualified)
                    && other.afterRaw == 0x11223344u && other.afterSet == expected, "raw sibling was restricted by the range");
            }
            Require(Target::WriteImpulseRangeBitsForAnalysis(invalidCapture, 0x3F800000, invalidCapture, Qualified)
                == Result::Completed && invalidCapture == before, "qualified setter depended on the previous range word");
        }
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
        CheckQualified();
        CheckRefusals();
        std::cout << "KickableIceImpulseRange: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
