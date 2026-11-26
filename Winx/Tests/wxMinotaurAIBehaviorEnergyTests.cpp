#include "Code/wxMinotaurAIBehavior.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Behavior = wxMinotaurAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Returned = Behavior::EnergyReturnForAnalysis;
    using Profile = Behavior::EnergyProfileForAnalysis;
    using Result = Behavior::EnergyResultForAnalysis;
    using Composed = Behavior::ComposedEnergyForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryEnergyDword1B4;
    constexpr auto RawQualified = Behavior::ProfileForAnalysis::PcCapturedPrimaryIdDwordAndCt2Byte;
    constexpr std::size_t IdOffset = 0x1B0;
    constexpr std::size_t EnergyOffset = 0x1B4;
    constexpr std::size_t AdjacentOffset = 0x1B8;
    constexpr std::size_t Ct2Offset = 0x1B9;
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

    Prefix Receiver(unsigned salt, std::uint32_t amplitude)
    {
        Prefix prefix{};
        for (std::size_t byte = 0; byte < prefix.size(); ++byte)
            prefix[byte] = std::uint8_t(salt * 13 + byte * 37);
        PutWord(prefix, 0, 0x006FFC48);
        PutWord(prefix, EnergyOffset, amplitude);
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
            Require(Behavior::ComposeEnergyForAnalysis(prefix, ParseHex(rawHex), result, Qualified)
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
            std::uint32_t before, argument, id;
            std::uint8_t adjacent, ct2;
            std::string_view beforeST0, afterST0;
        };
        constexpr Case cases[]{
            {0x3E99999Au, 0x3E99999Au, 0x00000001u, 0x00u, 0x00u, "00000000009a9999fd3f", "00000000009a9999fd3f"},
            {0x00000000u, 0x80000000u, 0x00000000u, 0xABu, 0x01u, "00000000000000000000", "00000000000000000080"},
            {0x80000000u, 0x00000000u, 0xFFFFFFFFu, 0x34u, 0x02u, "00000000000000000080", "00000000000000000000"},
            {0x3F800000u, 0xBF000000u, 0x80000000u, 0xFFu, 0x7Fu, "0000000000000080ff3f", "0000000000000080febf"},
            {0x00800000u, 0x7F7FFFFFu, 0x7FFFFFFFu, 0x12u, 0x80u, "0000000000000080813f", "0000000000ffffff7e40"},
            {0xFF7FFFFFu, 0x80800000u, 0xA5A5A5A5u, 0x80u, 0xFFu, "0000000000ffffff7ec0", "000000000000008081bf"},
            {0x80000000u, 0x80000000u, 0x7FC12345u, 0xA5u, 0x55u, "00000000000000000080", "00000000000000000080"},
            {0x3F800001u, 0x3DCCCCCDu, 0x01020304u, 0x55u, 0xAAu, "0000000000010080ff3f", "0000000000cdccccfb3f"},
        };
        unsigned salt = 0;
        for (const auto& trial : cases)
        {
            auto before = Receiver(salt++, trial.before);
            PutWord(before, IdOffset, trial.id);
            before[AdjacentOffset] = trial.adjacent;
            before[Ct2Offset] = trial.ct2;
            auto state = before;
            auto expected = before;
            PutWord(expected, EnergyOffset, trial.argument);
            Composed observed;
            Require(Behavior::ComposeEnergyForAnalysis(state, trial.argument, observed, Qualified)
                == Result::Completed, "qualified Energy operation refused");
            Require(observed.before == ParseBytes<10>(trial.beforeST0)
                && observed.after == ParseBytes<10>(trial.afterST0), "wrong physical x87 value");
            Require(state == before && observed.afterSet == expected, "raw field store changed unrelated bytes");
            Require(Behavior::WriteEnergyBitsForAnalysis(state, trial.argument, state, Qualified)
                == Result::Completed && state == expected, "aliased raw field store differs");
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
            Require(Behavior::ReadEnergyForAnalysis(before, raw, profile) == wanted && raw == oldSentinel,
                "getter refusal changed output or status");
            Require(Behavior::WriteEnergyBitsForAnalysis(before, 0x3F800000, written, profile) == wanted
                && written == prefixSentinel, "setter refusal changed output or status");
            Require(Behavior::ComposeEnergyForAnalysis(before, 0x3F800000, composed, profile) == wanted
                && unchanged(composed), "composed refusal changed output or status");
        };

        const auto before = Receiver(0, 0x3F800000);
        verify(before, Profile::Unqualified, Result::UnqualifiedProfile);
        verify(before, static_cast<Profile>(99), Result::UnqualifiedProfile);
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FF490);
        verify(wrongPrimary, Qualified, Result::CapturedPrimaryNotAdmitted);

        constexpr std::uint32_t rawOnly[]{
            0x00000001, 0x807FFFFF, 0x7F800000, 0xFF800000, 0x7FC12345, 0x7F800001,
        };
        for (const auto bits : rawOnly)
        {
            auto expected = before;
            PutWord(expected, EnergyOffset, bits);
            auto rawState = before;
            Require(Behavior::WriteEnergyBitsForAnalysis(rawState, bits, rawState, Qualified)
                == Result::Completed && rawState == expected, "raw Energy store inspected the argument");

            auto raw = oldSentinel;
            Composed composed{oldSentinel, prefixSentinel, newSentinel};
            Require(Behavior::ReadEnergyForAnalysis(rawState, raw, Qualified) == Result::CapturedEnergyNotAdmitted
                && raw == oldSentinel, "reader admitted an exceptional word or changed output");
            Require(Behavior::ComposeEnergyForAnalysis(rawState, 0x3F800000, composed, Qualified)
                == Result::CapturedEnergyNotAdmitted && unchanged(composed),
                "composed read admitted an exceptional captured word");
            Require(Behavior::ComposeEnergyForAnalysis(before, bits, composed, Qualified)
                == Result::ArgumentNotAdmitted && unchanged(composed),
                "composed write admitted an exceptional argument");

            auto expectedId = expected;
            PutWord(expectedId, IdOffset, 0x11223344);
            Behavior::ComposedIdForAnalysis id;
            Require(Behavior::ComposeRawIdForAnalysis(rawState, 0x11223344, id, RawQualified)
                && id.afterRaw == 0x11223344 && id.afterSet == expectedId, "ID depended on Energy bits");
            auto expectedCt2 = expected;
            expectedCt2[Ct2Offset] = 0xA5;
            Behavior::ComposedCt2ForAnalysis ct2;
            Require(Behavior::ComposeCt2LowByteForAnalysis(rawState, 0x123456A5, ct2, RawQualified)
                && ct2.afterRaw == 0xA5 && ct2.afterSet == expectedCt2, "Ct2 depended on Energy bits");
            Require(Behavior::WriteEnergyBitsForAnalysis(rawState, 0x3F800000, rawState, Qualified)
                == Result::Completed && rawState == before, "raw Energy store inspected the prior value");
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
        std::cout << "MinotaurAIBehaviorEnergy: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
