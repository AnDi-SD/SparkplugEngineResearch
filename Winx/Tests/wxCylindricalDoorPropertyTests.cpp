#include "Code/wxCylindricalDoor.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

using Door = winx::reconstruction::wxCylindricalDoor;
using Prefix = Door::CapturedPrefixForAnalysis;
using Property = Door::PropertyForAnalysis;
using Profile = Door::PropertyProfileForAnalysis;
using Result = Door::PropertyResultForAnalysis;

namespace
{
    unsigned contracts = 0;
    void Require(const bool value, const char* message)
    { ++contracts; if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    int Digit(const char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
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
    std::string Byte(const std::uint8_t value)
    {
        static constexpr char digits[] = "0123456789abcdef";
        return std::string(1, digits[value >> 4]) + digits[value & 15];
    }
    std::string Hex(const Prefix& input)
    { std::string text; for (const auto value : input) text += Byte(value); return text; }

    std::uint8_t Get(const Prefix& before, const Property property)
    {
        std::uint8_t output = 0xA5;
        Require(Door::ReadPropertyRawForAnalysis(before, property, output,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::Completed, "captured byte getter completes");
        return output;
    }
    Prefix Set(const Prefix& before, const Property property, const std::uint32_t argument)
    {
        Prefix after; after.fill(0x5C);
        Require(Door::WritePropertyRawForAnalysis(before, property, argument, after,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::Completed, "captured byte setter completes");
        auto expected = before;
        expected[property == Property::Locked ? 0x176 : 0x175] = std::uint8_t(argument);
        Require(after == expected, "setter truncation,neighbor field and opaque prefix preservation");
        auto alias = before;
        Require(Door::WritePropertyRawForAnalysis(alias, property, argument, alias,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::Completed && alias == after,
            "same-buffer setter output");
        Require(Door::WritePropertyRawForAnalysis(alias, property, argument, alias,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::Completed && alias == after,
            "repeated same-value setter is idempotent in represented state");
        return after;
    }
    std::string Compose(const Prefix& before, const std::uint32_t lockedArgument,
        const std::uint32_t proximityArgument)
    {
        const auto original = before;
        const auto oldLocked = Get(before, Property::Locked);
        const auto oldProximity = Get(before, Property::OpenedByProximity);
        const auto afterLocked = Set(before, Property::Locked, lockedArgument);
        const auto readLocked = Get(afterLocked, Property::Locked);
        const auto preservedProximity = Get(afterLocked, Property::OpenedByProximity);
        Require(readLocked == std::uint8_t(lockedArgument) && preservedProximity == oldProximity,
            "Locked readback preserves Opened by proximity");
        const auto afterBoth = Set(afterLocked, Property::OpenedByProximity, proximityArgument);
        const auto finalLocked = Get(afterBoth, Property::Locked);
        const auto finalProximity = Get(afterBoth, Property::OpenedByProximity);
        Require(finalLocked == readLocked && finalProximity == std::uint8_t(proximityArgument) && before == original,
            "second property readback preserves Locked and original input");
        return Byte(oldLocked) + ' ' + Byte(oldProximity) + ' ' + Hex(afterLocked) + ' '
            + Byte(readLocked) + ' ' + Byte(preservedProximity) + ' ' + Hex(afterBoth) + ' '
            + Byte(finalLocked) + ' ' + Byte(finalProximity);
    }
    Prefix Receiver()
    {
        Prefix before; before.fill(0xA5);
        const std::uint32_t primary = 0x00700AB0;
        std::memcpy(before.data(), &primary, 4);
        before[0x175] = 0xFF; before[0x176] = 0;
        return before;
    }
    void Units()
    {
        const auto before = Receiver();
        const std::array<std::array<std::uint32_t, 2>, 8> arguments{{
            {{0xDEADBEEF, 0x100}}, {{0, 1}}, {{2, 0xFF}}, {{0x7F, 0x80}},
            {{0xFFFFFFFF, 0xFFFFFF00}}, {{0x80000001, 0x7FFFFFFF}},
            {{0x01020304, 0xFFFF}}, {{0xA5A5A5A5, 0x5A5A5A5A}}
        }};
        for (const auto& pair : arguments) (void)Compose(before, pair[0], pair[1]);
        Prefix output; output.fill(0x5C); const auto refused = output;
        std::uint8_t byte = 0xC7;
        Require(Door::WritePropertyRawForAnalysis(before, Property::Locked, 1, output)
            == Result::UnqualifiedProfile && output == refused, "default setter refusal preserves prefix");
        Require(Door::ReadPropertyRawForAnalysis(before, Property::Locked, byte)
            == Result::UnqualifiedProfile && byte == 0xC7, "default getter refusal preserves byte");
        auto foreign = before; foreign[0] = 0;
        Require(Door::WritePropertyRawForAnalysis(foreign, Property::Locked, 1, output,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::CapturedPrimaryNotAdmitted && output == refused,
            "foreign primary setter refusal preserves output");
        Require(Door::ReadPropertyRawForAnalysis(foreign, Property::Locked, byte,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::CapturedPrimaryNotAdmitted && byte == 0xC7,
            "foreign primary getter refusal preserves output");
        const auto unknown = static_cast<Property>(255);
        Require(Door::WritePropertyRawForAnalysis(before, unknown, 1, output,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::UnknownProperty && output == refused,
            "unknown property setter refusal preserves output");
        Require(Door::ReadPropertyRawForAnalysis(before, unknown, byte,
            Profile::PcCapturedPrimaryRawByteProperties) == Result::UnknownProperty && byte == 0xC7,
            "unknown property getter refusal preserves output");
    }
}

int main(const int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream stream(line); std::string raw, extra;
            std::uint32_t locked = 0, proximity = 0; Prefix before;
            Require(bool(stream >> raw >> std::hex >> locked >> proximity) && !(stream >> extra)
                && Decode(raw, before), "exact composed source projection row");
            std::cout << Compose(before, locked, proximity) << '\n';
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) return EXIT_FAILURE;
    Units();
    std::cout << "CylindricalDoorProperties: " << contracts << " contracts passed\n";
    return EXIT_SUCCESS;
}
