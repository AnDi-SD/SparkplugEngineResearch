#include "Code/wxUpsieDaisyGuardAIBehavior.h"

#include <array>
#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
using Behavior = wxUpsieDaisyGuardAIBehavior;
using Prefix = Behavior::ScalarPrefixForAnalysis;
using Returned = Behavior::ScalarReturnForAnalysis;
using Property = Behavior::ScalarPropertyForAnalysis;
using Profile = Behavior::ScalarProfileForAnalysis;
using Result = Behavior::ScalarResultForAnalysis;
constexpr auto Qualified = Profile::PcCapturedPrimaryFiniteNormalOrZeroF32;

struct Fixture
{
    std::string_view name;
    Property property;
    std::size_t offset;
    std::uint32_t initialBits;
    std::string_view initialReturn;
};

constexpr Fixture Fixtures[]{
    {"walk", Property::WalkSpeed, 0x164, 0x3F800000, "0000000000000080ff3f"},
    {"turn", Property::TurnSpeed, 0x188, 0xBF800000, "0000000000000080ffbf"},
    {"visionCos", Property::VisionCos, 0x194, 0, "00000000000000000000"},
    {"visionLengthSquared", Property::VisionLengthSquared, 0x198, 0x80000000, "00000000000000000080"},
};

unsigned checks{};

void Require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
    ++checks;
}

std::uint32_t ReadHex(std::string_view text)
{
    std::uint32_t value{};
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        throw std::runtime_error("expected hexadecimal input");
    return value;
}

template<std::size_t N>
std::string Hex(const std::array<std::uint8_t, N>& bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(2 * bytes.size());
    for (const auto value : bytes)
    {
        output += digits[value >> 4];
        output += digits[value & 15];
    }
    return output;
}

Prefix ParsePrefix(std::string_view text)
{
    Prefix result{};
    if (text.size() != 2 * result.size())
        throw std::runtime_error("expected a 412-byte hexadecimal prefix");
    for (std::size_t byte = 0; byte < result.size(); ++byte)
        result[byte] = std::uint8_t(ReadHex(text.substr(2 * byte, 2)));
    return result;
}

void PutWord(Prefix& prefix, std::size_t offset, std::uint32_t bits)
{
    for (unsigned byte = 0; byte < 4; ++byte)
        prefix[offset + byte] = std::uint8_t(bits >> (8 * byte));
}

Prefix Receiver()
{
    Prefix result{};
    for (std::size_t byte = 0; byte < result.size(); ++byte)
        result[byte] = std::uint8_t(17 + byte * 37);
    PutWord(result, 0, 0x007001E0);
    for (const auto& fixture : Fixtures)
        PutWord(result, fixture.offset, fixture.initialBits);
    return result;
}

Returned ReadScalar(const Prefix& prefix, Property property)
{
    Returned result;
    result.fill(0x5C);
    Require(Behavior::ReadScalarForAnalysis(prefix, property, result, Qualified) == Result::Completed,
        "selected scalar getter refused captured prefix");
    return result;
}

Prefix WriteScalar(const Prefix& prefix, Property property, std::uint32_t bits)
{
    Prefix result;
    result.fill(0xC7);
    Require(Behavior::WriteScalarBitsForAnalysis(prefix, property, bits, result, Qualified) == Result::Completed,
        "selected scalar setter refused captured prefix");
    return result;
}

Property ParseProperty(std::string_view name)
{
    for (const auto& fixture : Fixtures)
    {
        if (fixture.name == name)
            return fixture.property;
    }
    throw std::runtime_error("unknown guard scalar name");
}

int Replay()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream fields(line);
        std::string propertyName, prefixHex, argumentHex, extra;
        if (!(fields >> propertyName >> prefixHex >> argumentHex) || fields >> extra || argumentHex.size() != 8)
            throw std::runtime_error("expected a guard scalar name, captured prefix and eight argument digits");
        const auto property = ParseProperty(propertyName);
        const auto before = ParsePrefix(prefixHex);
        const auto initial = ReadScalar(before, property);
        const auto after = WriteScalar(before, property, ReadHex(argumentHex));
        const auto final = ReadScalar(after, property);
        std::cout << Hex(initial) << ' ' << Hex(after) << ' ' << Hex(final) << '\n';
    }
    if (!std::cin.eof() || !std::cout)
        throw std::runtime_error("replay input or output failed");
    return 0;
}

void CheckFiniteValues()
{
    struct Golden { std::uint32_t bits; const char* extended; };
    constexpr Golden values[]{
        {0x00000000, "00000000000000000000"}, {0x80000000, "00000000000000000080"},
        {0x3F800000, "0000000000000080ff3f"}, {0xBF800000, "0000000000000080ffbf"},
        {0x00800000, "0000000000000080813f"}, {0x80800000, "000000000000008081bf"},
        {0x7F7FFFFF, "0000000000ffffff7e40"}, {0xFF7FFFFF, "0000000000ffffff7ec0"},
    };
    const auto before = Receiver();
    for (const auto& fixture : Fixtures)
    {
        const auto property = fixture.property;
        Require(Hex(ReadScalar(before, property)) == fixture.initialReturn, "wrong scalar selector");
        for (const auto& value : values)
        {
            auto expected = before;
            PutWord(expected, fixture.offset, value.bits);
            const auto after = WriteScalar(before, property, value.bits);
            Require(after == expected, "setter did not preserve non-selected bytes");
            Require(Hex(ReadScalar(after, property)) == value.extended, "incorrect physical x87 return bytes");
            auto aliased = before;
            Require(Behavior::WriteScalarBitsForAnalysis(aliased, property, value.bits, aliased, Qualified)
                == Result::Completed && aliased == after, "aliased setter did not reproduce the staged state");
        }
    }
}

void CheckRefusals()
{
    Prefix written;
    written.fill(0xC7);
    const auto writtenSentinel = written;
    Returned returned;
    returned.fill(0x5C);
    const auto returnedSentinel = returned;
    const auto before = Receiver();
    const auto verify = [&](const Prefix& input, Property property, Profile profile, Result expected)
    {
        Require(Behavior::ReadScalarForAnalysis(input, property, returned, profile) == expected
            && returned == returnedSentinel, "getter refusal changed output or reason");
        Require(Behavior::WriteScalarBitsForAnalysis(input, property, 0, written, profile) == expected
            && written == writtenSentinel, "setter refusal changed output or reason");
    };
    verify(before, Property::WalkSpeed, Profile::Unqualified, Result::UnqualifiedProfile);
    verify(before, Property::TurnSpeed, static_cast<Profile>(99), Result::UnqualifiedProfile);
    auto wrongPrimary = before;
    wrongPrimary[0] ^= 1;
    verify(wrongPrimary, Property::WalkSpeed, Qualified, Result::CapturedPrimaryNotAdmitted);
    verify(before, static_cast<Property>(99), Qualified, Result::UnknownProperty);
    for (const auto bits : {1u, 0x80000001u, 0x7F800000u, 0xFF800000u, 0x7FC00001u, 0x7F800001u})
    {
        for (const auto& fixture : Fixtures)
        {
            const auto property = fixture.property;
            Require(Behavior::WriteScalarBitsForAnalysis(before, property, bits, written, Qualified)
                == Result::ArgumentNotAdmitted && written == writtenSentinel, "invalid argument changed output");
            for (const auto& captured : Fixtures)
            {
                auto invalid = before;
                PutWord(invalid, captured.offset, bits);
                verify(invalid, property, Qualified, Result::CapturedScalarNotAdmitted);
            }
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
        CheckFiniteValues();
        CheckRefusals();
        std::cout << "UpsieDaisyGuardAIBehaviorScalar: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
