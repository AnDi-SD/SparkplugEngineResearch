#include "Code/wxIntelliCam.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

using Facade = wxIntelliCam;
using Prefix = Facade::CapturedPrefixForAnalysis;
using Returned = Facade::ScalarReturnForAnalysis;
using Property = Facade::ScalarPropertyForAnalysis;
using Profile = Facade::ScalarProfileForAnalysis;
using Result = Facade::ScalarResultForAnalysis;
constexpr std::array<std::size_t, 10> Offsets{0x3E4, 0x3E0, 0x18C, 0x1A8, 0x170, 0x188, 0x1A4, 0x184, 0x1A0, 0x14C};
constexpr auto Qualified = Profile::PcCapturedPrimaryFiniteNormalOrZeroF32;

namespace
{
unsigned contracts = 0;
void Require(const bool value, const char* message)
{
    ++contracts;
    if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
int Digit(const char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
template<std::size_t N> std::string Hex(const std::array<std::uint8_t, N>& input)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string output;
    for (const auto value : input) { output += digits[value >> 4]; output += digits[value & 15]; }
    return output;
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
{
    for (unsigned index = 0; index != 4; ++index) prefix[offset + index] = std::uint8_t(word >> (index * 8));
}
Returned Get(const Prefix& prefix, const Property property)
{
    Returned output; output.fill(0x5C);
    Require(Facade::ReadScalarForAnalysis(prefix, property, output, Qualified) == Result::Completed,
        "selected getter supplies physical ten-byte payload");
    return output;
}
std::string Compose(const Prefix& before, const std::array<std::uint32_t, 10>& arguments)
{
    auto current = before;
    std::string row;
    for (unsigned property = 0; property != Offsets.size(); ++property)
    {
        const auto selected = static_cast<Property>(property);
        if (property) row += ' ';
        row += Hex(Get(current, selected));
        for (unsigned repetition = 0; repetition != 2; ++repetition)
        {
            Prefix after; after.fill(0xC7);
            Require(Facade::WriteScalarBitsForAnalysis(current, selected, arguments[property], after, Qualified)
                == Result::Completed, "raw four-byte scalar setter completes");
            auto expected = current;
            Put(expected, Offsets[property], arguments[property]);
            Require(after == expected, "every other prefix byte is preserved including all old RAW fields");
            auto alias = current;
            Require(Facade::WriteScalarBitsForAnalysis(alias, selected, arguments[property], alias, Qualified)
                == Result::Completed && alias == after, "scalar prefix input/output may alias");
            row += ' ' + Hex(after) + ' ' + Hex(Get(after, selected));
            current = after;
        }
    }
    return row;
}
Prefix Receiver()
{
    Prefix before; before.fill(0xA5); Put(before, 0, 0x6F522C);
    for (const auto offset : Offsets) Put(before, offset, 0);
    return before;
}
void Units()
{
    const std::array<std::pair<std::uint32_t, const char*>, 8> golden{{
        {0, "00000000000000000000"}, {0x80000000, "00000000000000000080"},
        {0x3F800000, "0000000000000080ff3f"}, {0xBF800000, "0000000000000080ffbf"},
        {0x00800000, "0000000000000080813f"}, {0x80800000, "000000000000008081bf"},
        {0x7F7FFFFF, "0000000000ffffff7e40"}, {0xFF7FFFFF, "0000000000ffffff7ec0"}
    }};
    for (unsigned property = 0; property != Offsets.size(); ++property)
    {
        const auto selected = static_cast<Property>(property);
        for (const auto& example : golden)
        {
            auto before = Receiver(); Put(before, Offsets[property], example.first);
            Require(Hex(Get(before, selected)) == example.second, "independent binary32/extended golden value including sign");
        }
    }
    for (const auto& example : golden)
    {
        std::array<std::uint32_t, 10> arguments{}; arguments.fill(example.first);
        (void)Compose(Receiver(), arguments);
    }
    const auto before = Receiver();
    Prefix output; output.fill(0xC7); const auto sentinel = output;
    Returned returned; returned.fill(0x5C); const auto returnedSentinel = returned;
    Require(Facade::WriteScalarBitsForAnalysis(before, Property::MouseRotationSpeed, 0, output) == Result::UnqualifiedProfile
        && output == sentinel, "default scalar setter refusal preserves output");
    Require(Facade::ReadScalarForAnalysis(before, Property::MouseRotationSpeed, returned) == Result::UnqualifiedProfile
        && returned == returnedSentinel, "default scalar getter refusal preserves output");
    auto foreign = before; foreign[0] = 0;
    Require(Facade::ReadScalarForAnalysis(foreign, Property::MouseRotationSpeed, returned, Qualified) == Result::CapturedPrimaryNotAdmitted
        && returned == returnedSentinel, "foreign captured primary refusal");
    Require(Facade::WriteScalarBitsForAnalysis(before, static_cast<Property>(255), 0, output, Qualified) == Result::UnknownProperty
        && output == sentinel, "unknown scalar property refusal");
    for (unsigned property = 0; property != Offsets.size(); ++property)
    {
        const auto selected = static_cast<Property>(property);
        for (const std::uint32_t word : {1U, 0x80000001U, 0x7F800000U, 0xFF800000U, 0x7FC00001U, 0x7F800001U})
        {
            Require(Facade::WriteScalarBitsForAnalysis(before, selected, word, output, Qualified) == Result::ArgumentNotAdmitted
                && output == sentinel, "out-of-profile argument refusal preserves output");
            auto outside = before; Put(outside, Offsets[property], word);
            Require(Facade::ReadScalarForAnalysis(outside, selected, returned, Qualified) == Result::CapturedScalarNotAdmitted
                && returned == returnedSentinel, "selected scalar domain refusal preserves return");
            const auto other = static_cast<Property>((property + 1) % Offsets.size());
            Require(Facade::ReadScalarForAnalysis(outside, other, returned, Qualified) == Result::CapturedScalarNotAdmitted
                && returned == returnedSentinel, "all ten captured scalar fields belong to the finite profile");

        }
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
            std::istringstream stream(line); std::string raw, extra; Prefix before;
            std::array<std::uint32_t, 10> arguments{};
            bool parsed = bool(stream >> raw >> std::hex);
            for (auto& argument : arguments) parsed = parsed && bool(stream >> argument);
            Require(parsed && !(stream >> extra) && Decode(raw, before), "exact scalar retained-group source input");
            std::cout << Compose(before, arguments) << '\n';
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) return EXIT_FAILURE;
    Units(); std::cout << "IntelliCamScalar: " << contracts << " contracts passed\n";
    return EXIT_SUCCESS;
}
