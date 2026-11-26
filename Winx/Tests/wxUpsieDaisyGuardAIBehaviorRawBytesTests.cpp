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
using Prefix = Behavior::BytePrefixForAnalysis;
using Property = Behavior::BytePropertyForAnalysis;
using Profile = Behavior::ByteProfileForAnalysis;
constexpr auto Qualified = Profile::PcCapturedPrimaryRawBytes1D4And1D5;

struct Fixture
{
    std::string_view name;
    Property property;
    std::size_t offset;
};

constexpr Fixture Fixtures[]{
    {"waitForSignal", Property::WaitForSignal, 0x1D4},
    {"followFloor", Property::FollowFloor, 0x1D5},
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
        throw std::runtime_error("expected a 470-byte hexadecimal prefix");
    for (std::size_t byte = 0; byte < result.size(); ++byte)
        result[byte] = std::uint8_t(ReadHex(text.substr(2 * byte, 2)));
    return result;
}

Property ParseProperty(std::string_view name)
{
    for (const auto& fixture : Fixtures)
    {
        if (fixture.name == name)
            return fixture.property;
    }
    throw std::runtime_error("unknown guard byte property");
}

Prefix Receiver()
{
    Prefix result{};
    for (std::size_t byte = 0; byte < result.size(); ++byte)
        result[byte] = std::uint8_t(17 + byte * 37);
    result[0] = 0xE0;
    result[1] = 0x01;
    result[2] = 0x70;
    result[3] = 0;
    return result;
}

std::uint8_t ReadByte(const Prefix& prefix, Property property)
{
    std::uint8_t result = 0x5C;
    Require(Behavior::ReadBytePropertyForAnalysis(prefix, property, result, Qualified),
        "selected byte getter refused captured prefix");
    return result;
}

Prefix WriteByte(const Prefix& prefix, Property property, std::uint32_t argument)
{
    Prefix result;
    result.fill(0xC7);
    Require(Behavior::WriteLowBytePropertyForAnalysis(prefix, property, argument, result, Qualified),
        "selected byte setter refused captured prefix");
    return result;
}

int Replay()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream fields(line);
        std::string propertyName, prefixHex, argumentHex, extra;
        if (!(fields >> propertyName >> prefixHex >> argumentHex) || fields >> extra || argumentHex.size() != 8)
            throw std::runtime_error("expected a guard byte selector, captured prefix and eight argument digits");
        const auto property = ParseProperty(propertyName);
        const auto before = ParsePrefix(prefixHex);
        const auto initial = ReadByte(before, property);
        const auto after = WriteByte(before, property, ReadHex(argumentHex));
        const auto final = ReadByte(after, property);
        std::cout << Hex(std::array<std::uint8_t, 1>{initial}) << ' ' << Hex(after)
            << ' ' << Hex(std::array<std::uint8_t, 1>{final}) << '\n';
    }
    if (!std::cin.eof() || !std::cout)
        throw std::runtime_error("replay input or output failed");
    return 0;
}

void CheckByteValues()
{
    for (const auto& fixture : Fixtures)
    {
        for (std::uint32_t lowByte = 0; lowByte < 256; ++lowByte)
        {
            auto before = Receiver();
            before[fixture.offset] = std::uint8_t(lowByte ^ 0xA5u);
            const auto original = before;
            const auto argument = (0xABCDEF00u ^ (lowByte << 8)) | lowByte;
            Require(ReadByte(before, fixture.property) == before[fixture.offset], "getter normalized selected byte");
            auto expected = before;
            expected[fixture.offset] = std::uint8_t(lowByte);
            const auto after = WriteByte(before, fixture.property, argument);
            Require(after == expected && before == original, "setter changed a non-selected byte or input");
            Require(ReadByte(after, fixture.property) == std::uint8_t(lowByte), "setter lost low argument byte");
            auto aliased = before;
            Require(Behavior::WriteLowBytePropertyForAnalysis(aliased, fixture.property, argument, aliased, Qualified)
                && aliased == after, "aliased setter did not reproduce the staged state");
        }
    }
}

void CheckRefusals()
{
    Prefix written;
    written.fill(0xC7);
    const auto sentinel = written;
    std::uint8_t raw = 0x5C;
    const auto before = Receiver();
    const auto verify = [&](const Prefix& input, Property property, Profile profile)
    {
        Require(!Behavior::ReadBytePropertyForAnalysis(input, property, raw, profile) && raw == 0x5C,
            "getter refusal changed output");
        Require(!Behavior::WriteLowBytePropertyForAnalysis(input, property, 0xFFFFFFFFu, written, profile)
            && written == sentinel, "setter refusal changed output");
    };
    verify(before, Property::WaitForSignal, Profile::Unqualified);
    verify(before, Property::FollowFloor, static_cast<Profile>(99));
    auto wrongPrimary = before;
    wrongPrimary[0] ^= 1;
    verify(wrongPrimary, Property::WaitForSignal, Qualified);
    verify(before, static_cast<Property>(99), Qualified);
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
        CheckByteValues();
        CheckRefusals();
        std::cout << "UpsieDaisyGuardAIBehaviorRawBytes: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
