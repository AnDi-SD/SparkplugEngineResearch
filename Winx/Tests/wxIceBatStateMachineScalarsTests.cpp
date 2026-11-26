#include "Code/wxIceBatStateMachine.h"

#include <charconv>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Machine = wxIceBatStateMachine;
    using Prefix = Machine::CapturedPrefixForAnalysis;
    using Returned = Machine::ScalarReturnForAnalysis;
    using Profile = Machine::ProfileForAnalysis;
    using Result = Machine::ResultForAnalysis;
    using Composed = Machine::ComposedScalarForAnalysis;
    constexpr auto Qualified = Profile::PcCapturedPrimaryScalarDwords22CAnd230;
    using Property = Machine::PropertyForAnalysis;

    struct Field
    {
        std::string_view name;
        Property property;
        std::size_t offset;
    };

    constexpr Field Fields[]{
        {"trigger", Property::TriggerRadius, 0x22C},
        {"speed", Property::MovementSpeed, 0x230},
    };
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

    const Field& FindField(std::string_view selector)
    {
        for (const auto& field : Fields)
            if (field.name == selector)
                return field;
        throw std::runtime_error("unknown scalar selector");
    }

    Prefix Receiver(unsigned salt, const std::array<std::uint32_t, 2>& words)
    {
        Prefix prefix{};
        for (std::size_t byte = 0; byte < prefix.size(); ++byte)
            prefix[byte] = std::uint8_t(salt * 13 + byte * 37);
        PutWord(prefix, 0, 0x006FAA58);
        for (std::size_t index = 0; index < words.size(); ++index)
            PutWord(prefix, Fields[index].offset, words[index]);
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream stream(line);
            std::string selector, prefixHex, rawHex, extra;
            if (!(stream >> selector >> prefixHex >> rawHex) || stream >> extra || rawHex.size() != 8)
                throw std::runtime_error("expected a scalar selector, captured prefix and eight argument digits");
            const auto prefix = ParseBytes<Prefix{}.size()>(prefixHex);
            Composed result;
            Require(Machine::ComposeScalarForAnalysis(prefix, FindField(selector).property,
                    ParseHex(rawHex), result, Qualified) == Result::Completed,
                "qualified replay input was refused");
            std::cout << Hex(result.before) << ' ' << Hex(result.afterSet) << ' ' << Hex(result.after) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckRetainedScalars()
    {
        struct Trial
        {
            std::uint32_t before, argument;
            std::string_view beforeST0, afterST0;
        };
        constexpr std::array<Trial, 2> cases[]{
            {{
                {0x00000000u, 0x00000000u, "00000000000000000000", "00000000000000000000"},
                {0x00000000u, 0x00000000u, "00000000000000000000", "00000000000000000000"},
            }},
            {{
                {0x3F800000u, 0x80000000u, "0000000000000080ff3f", "00000000000000000080"},
                {0x80000000u, 0x00000000u, "00000000000000000080", "00000000000000000000"},
            }},
            {{
                {0x80000000u, 0x00000000u, "00000000000000000080", "00000000000000000000"},
                {0x40000000u, 0xC0600000u, "00000000000000800040", "00000000000000e000c0"},
            }},
            {{
                {0x3DCCCCCDu, 0xC0200000u, "0000000000cdccccfb3f", "00000000000000a000c0"},
                {0xBF000000u, 0x3F000000u, "0000000000000080febf", "0000000000000080fe3f"},
            }},
            {{
                {0x00800000u, 0x7F7FFFFFu, "0000000000000080813f", "0000000000ffffff7e40"},
                {0x7F7FFFFFu, 0xFF7FFFFFu, "0000000000ffffff7e40", "0000000000ffffff7ec0"},
            }},
            {{
                {0xFF7FFFFFu, 0x80800000u, "0000000000ffffff7ec0", "000000000000008081bf"},
                {0x80800000u, 0x00800000u, "000000000000008081bf", "0000000000000080813f"},
            }},
            {{
                {0xBF800000u, 0xBF800000u, "0000000000000080ffbf", "0000000000000080ffbf"},
                {0x3F800001u, 0x3F800001u, "0000000000010080ff3f", "0000000000010080ff3f"},
            }},
            {{
                {0x3F800001u, 0x41200000u, "0000000000010080ff3f", "00000000000000a00240"},
                {0x00800000u, 0x80000000u, "0000000000000080813f", "00000000000000000080"},
            }},
        };
        unsigned salt = 0;
        for (const auto& fixture : cases)
        {
            std::array<std::uint32_t, 2> initial{};
            for (std::size_t index = 0; index < initial.size(); ++index)
                initial[index] = fixture[index].before;
            auto state = Receiver(salt++, initial);
            for (std::size_t index = 0; index < fixture.size(); ++index)
            {
                const auto& trial = fixture[index];
                const auto& field = Fields[index];
                const auto before = state;
                auto expected = state;
                PutWord(expected, field.offset, trial.argument);
                Composed observed;
                Require(Machine::ComposeScalarForAnalysis(state, field.property, trial.argument, observed, Qualified)
                    == Result::Completed, "typed scalar operation refused");
                Require(observed.before == ParseBytes<10>(trial.beforeST0)
                    && observed.after == ParseBytes<10>(trial.afterST0), "wrong physical x87 value");
                Require(state == before && observed.afterSet == expected, "scalar store changed unrelated bytes");
                auto alias = state;
                Require(Machine::WriteScalarBitsForAnalysis(alias, field.property, trial.argument, alias, Qualified)
                    == Result::Completed && alias == expected, "aliased raw scalar store differs");
                state = observed.afterSet;
            }
        }
    }

    void CheckRefusalsAndRawStores()
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
        const auto verify = [&](const Prefix& before, Property property, Profile profile, Result wanted)
        {
            auto raw = oldSentinel;
            auto written = prefixSentinel;
            Composed composed{oldSentinel, prefixSentinel, newSentinel};
            Require(Machine::ReadScalarForAnalysis(before, property, raw, profile) == wanted && raw == oldSentinel,
                "getter refusal changed output or status");
            Require(Machine::WriteScalarBitsForAnalysis(before, property, 0x3F800000, written, profile) == wanted
                && written == prefixSentinel, "setter refusal changed output or status");
            Require(Machine::ComposeScalarForAnalysis(before, property, 0x3F800000, composed, profile) == wanted
                && unchanged(composed), "composed refusal changed output or status");
        };

        const auto before = Receiver(0, {0x3F800000, 0x3F800000});
        verify(before, Property::TriggerRadius, Profile::Unqualified, Result::UnqualifiedProfile);
        verify(before, Property::MovementSpeed, static_cast<Profile>(99), Result::UnqualifiedProfile);
        verify(before, static_cast<Property>(99), Qualified, Result::UnknownProperty);
        auto wrongPrimary = before;
        PutWord(wrongPrimary, 0, 0x006FFC48);
        verify(wrongPrimary, Property::TriggerRadius, Qualified, Result::CapturedPrimaryNotAdmitted);

        constexpr std::uint32_t rawOnly[]{
            0x00000001, 0x807FFFFF, 0x7F800000, 0xFF800000, 0x7FC12345, 0x7F800001,
        };
        for (const auto bits : rawOnly)
        {
            for (std::size_t index = 0; index < std::size(Fields); ++index)
            {
                const auto& selected = Fields[index];
                const auto& neighbor = Fields[(index + 1) % std::size(Fields)];
                auto rawState = before;
                auto expected = before;
                PutWord(expected, selected.offset, bits);
                Require(Machine::WriteScalarBitsForAnalysis(rawState, selected.property, bits, rawState, Qualified)
                    == Result::Completed && rawState == expected, "raw setter inspected the argument");

                auto raw = oldSentinel;
                Composed composed{oldSentinel, prefixSentinel, newSentinel};
                Require(Machine::ReadScalarForAnalysis(rawState, selected.property, raw, Qualified)
                    == Result::CapturedScalarNotAdmitted && raw == oldSentinel,
                    "getter admitted an exceptional selected word");
                Require(Machine::ComposeScalarForAnalysis(rawState, selected.property, 0x3F800000, composed, Qualified)
                    == Result::CapturedScalarNotAdmitted && unchanged(composed),
                    "composed path admitted an exceptional selected word");
                Require(Machine::ComposeScalarForAnalysis(before, selected.property, bits, composed, Qualified)
                    == Result::ArgumentNotAdmitted && unchanged(composed),
                    "composed path admitted an exceptional argument");

                PutWord(expected, neighbor.offset, 0x80000000);
                Require(Machine::ComposeScalarForAnalysis(rawState, neighbor.property, 0x80000000, composed, Qualified)
                    == Result::Completed && composed.afterSet == expected, "operation inspected an unrelated scalar");
                Require(Machine::WriteScalarBitsForAnalysis(rawState, selected.property, 0x3F800000, rawState, Qualified)
                    == Result::Completed && rawState == before, "raw setter inspected the old word");
            }
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
        CheckRetainedScalars();
        CheckRefusalsAndRawStores();
        std::cout << "IceBatStateMachineScalars: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
