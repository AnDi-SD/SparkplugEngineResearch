#include "Code/wxKikoHole.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

using Facade = winx::reconstruction::wxKikoHole;
using Prefix = Facade::CapturedPrefixForAnalysis;
using Returned = Facade::ExtendedReturnForAnalysis;
using Property = Facade::PropertyForAnalysis;
using Profile = Facade::ProfileForAnalysis;
using Result = Facade::ResultForAnalysis;

namespace
{
    unsigned contracts = 0;
    void Require(const bool value, const char* message)
    { ++contracts; if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    int Digit(const char value)
    { return value >= '0' && value <= '9' ? value - '0' : value >= 'a' && value <= 'f' ? value - 'a' + 10 : value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1; }
    template<std::size_t N> std::string Hex(const std::array<std::uint8_t, N>& input)
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string text;
        for (const auto value : input) { text += digits[value >> 4]; text += digits[value & 15]; }
        return text;
    }
    bool Decode(const std::string& text, Prefix& output)
    {
        if (text.size() != output.size() * 2) return false;
        for (std::size_t index = 0; index != output.size(); ++index)
        {
            const auto high = Digit(text[index * 2]), low = Digit(text[index * 2 + 1]);
            if (high < 0 || low < 0) return false;
            output[index] = std::uint8_t(high * 16 + low);
        }
        return true;
    }
    void Put(Prefix& prefix, const std::size_t offset, const std::uint32_t word)
    { for (unsigned index = 0; index != 4; ++index) prefix[offset + index] = std::uint8_t(word >> (index * 8)); }
    Returned Get(const Prefix& prefix, const Property property)
    {
        Returned output; output.fill(0x5C);
        Require(Facade::ReadScalarForAnalysis(prefix, property, output, Profile::PcCapturedPrimaryFiniteNormalOrZeroF32)
            == Result::Completed, "captured scalar getter returns exact extended payload");
        return output;
    }
    std::string Compose(const Prefix& before, const std::array<std::uint32_t, 1>& arguments)
    {
        auto current = before; std::string row;
        for (unsigned property = 0; property != 1; ++property)
        {
            const auto selected = static_cast<Property>(property);
            const auto old = Get(current, selected); Prefix after; after.fill(0xC7);
            Require(Facade::WriteScalarBitsForAnalysis(current, selected, arguments[property], after,
                Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::Completed, "raw DWORD setter completes");
            auto expected = current; Put(expected, 0x140, arguments[property]);
            Require(after == expected, "raw four-byte store preserves all other fields and opaque prefix");
            auto alias = current;
            Require(Facade::WriteScalarBitsForAnalysis(alias, selected, arguments[property], alias,
                Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::Completed && alias == after, "prefix output may alias input");
            const auto returned = Get(after, selected);
            if (property) row += ' ';
            row += Hex(old) + ' ' + Hex(after) + ' ' + Hex(returned); current = after;
        }
        return row;
    }
    Prefix Receiver()
    {
        Prefix before; before.fill(0xA5); Put(before, 0, 0x6F633C);
        for (unsigned property = 0; property != 1; ++property) Put(before, 0x140, 0);
        return before;
    }
    void Units()
    {
        const std::array<std::pair<std::uint32_t, const char*>, 6> golden{{
            {0, "00000000000000000000"}, {0x80000000, "00000000000000000080"},
            {0x3F800000, "0000000000000080ff3f"}, {0xBF800000, "0000000000000080ffbf"},
            {0x00800000, "0000000000000080813f"}, {0x7F7FFFFF, "0000000000ffffff7e40"}
        }};
        for (const auto& example : golden)
        {
            auto before = Receiver(); Put(before, 0x140, example.first);
            Require(Hex(Get(before, Property::Radius)) == example.second, "known binary32 to extended payload including signed zero");
        }
        const auto before = Receiver();
        for(const auto& example : golden) (void)Compose(before, {example.first});
        Prefix output; output.fill(0xC7); const auto sentinel = output;
        Returned returned; returned.fill(0x5C); const auto returnedSentinel = returned;
        Require(Facade::WriteScalarBitsForAnalysis(before, Property::Radius, 0, output) == Result::UnqualifiedProfile && output == sentinel, "default setter refusal");
        Require(Facade::ReadScalarForAnalysis(before, Property::Radius, returned) == Result::UnqualifiedProfile && returned == returnedSentinel, "default getter refusal");
        auto foreign = before; foreign[0] = 0;
        Require(Facade::ReadScalarForAnalysis(foreign, Property::Radius, returned, Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::CapturedPrimaryNotAdmitted && returned == returnedSentinel, "foreign primary refusal");
        Require(Facade::WriteScalarBitsForAnalysis(before, static_cast<Property>(255), 0, output, Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::UnknownProperty && output == sentinel, "unknown property refusal");
        for (const std::uint32_t word : {1U, 0x80000001U, 0x7F800000U, 0xFF800000U, 0x7FC00001U, 0x7F800001U})
        {
            Require(Facade::WriteScalarBitsForAnalysis(before, Property::Radius, word, output, Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::ArgumentNotAdmitted && output == sentinel, "out-of-profile argument refusal preserves output");
            auto outside = before; Put(outside, 0x140, word);
            Require(Facade::ReadScalarForAnalysis(outside, Property::Radius, returned, Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) == Result::CapturedScalarNotAdmitted && returned == returnedSentinel, "explicit selected-Radius profile refusal preserves return");
        }
    }
}

int main(const int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream stream(line); std::string raw, extra; Prefix before; std::array<std::uint32_t, 1> arguments{};
            Require(bool(stream >> raw >> std::hex >> arguments[0])
                && !(stream >> extra) && Decode(raw, before), "exact scalar source row");
            std::cout << Compose(before, arguments) << '\n';
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) return EXIT_FAILURE;
    Units(); std::cout << "KikoHoleRadius: " << contracts << " contracts passed\n";
    return EXIT_SUCCESS;
}
