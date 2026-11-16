#include "../Code/wxCabinetPuzzle.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using F = wxCabinetPuzzle;
using Prefix = F::CapturedPrefixForAnalysis;
constexpr auto Profile = F::ProfileForAnalysis::PcCapturedPrimaryRawDwords13414815C170;
constexpr std::array<F::PropertyForAnalysis, 4> Properties = {F::PropertyForAnalysis::Digit4Raw170,
    F::PropertyForAnalysis::Digit3Raw15C, F::PropertyForAnalysis::Digit2Raw148, F::PropertyForAnalysis::Digit1Raw134};
constexpr std::array<std::size_t, 4> Offsets = {0x170, 0x15C, 0x148, 0x134};
void Put(Prefix& p, std::size_t offset, std::uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte) p[offset + byte] = std::uint8_t(value >> (byte * 8));
}
std::string Hex(const Prefix& p)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (const auto byte : p) out << std::setw(2) << unsigned(byte);
    return out.str();
}
// Replay DWORD tokens contain four bytes in memory order, just like prefix tokens.
std::string WordHex(std::uint32_t word)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (unsigned byte = 0; byte < 4; ++byte) out << std::setw(2) << unsigned(std::uint8_t(word >> (byte * 8)));
    return out.str();
}
Prefix ParsePrefix(const std::string& value)
{
    Prefix p{};
    if (value.size() != p.size() * 2 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("prefix");
    for (std::size_t byte = 0; byte < p.size(); ++byte) p[byte] = std::uint8_t(std::stoul(value.substr(byte * 2, 2), nullptr, 16));
    return p;
}
std::uint32_t ParseWord(const std::string& value)
{
    if (value.size() != 8 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("word");
    std::uint32_t result = 0;
    for (unsigned byte = 0; byte < 4; ++byte) result |= std::uint32_t(std::stoul(value.substr(byte * 2, 2), nullptr, 16)) << (byte * 8);
    return result;
}
std::string Render(const F::ComposedPropertiesForAnalysis& r)
{
    std::string result;
    for (std::size_t index = 0; index < 4; ++index)
    {
        if (index) result += ' ';
        result += WordHex(r.rawGetterResults[index * 2]) + " " + Hex(r.afterSet[index]) + " " + WordHex(r.rawGetterResults[index * 2 + 1]);
    }
    return result;
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay")
        {
            std::string line;
            while (std::getline(std::cin, line))
            {
                std::istringstream input(line); std::string prefix, extra; std::array<std::string, 4> words;
                if (!(input >> prefix >> words[0] >> words[1] >> words[2] >> words[3]) || (input >> extra)) return 2;
                std::array<std::uint32_t, 4> arguments{};
                for (std::size_t index = 0; index < 4; ++index) arguments[index] = ParseWord(words[index]);
                F::ComposedPropertiesForAnalysis result{};
                if (!F::ComposeRawPropertiesForAnalysis(ParsePrefix(prefix), arguments, result, Profile)) return 3;
                std::cout << Render(result) << '\n';
            }
            return 0;
        }
        if (argc != 1) return 2;
        unsigned checks = 0;
        auto require = [&](bool condition) { if (!condition) throw std::runtime_error("contract"); ++checks; };
        constexpr std::uint32_t words[] = {0, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x7F800000u, 0xFF800000u,
            0x7FC00001u, 0x80000001u, 1, 0x01020304u, 0xA5A5A5A5u, 0x5A5A5A5Au, 0x12345678u, 0xFEDCBA98u, 0x00010000u, 0xFFFFFFFFu};
        for (unsigned fixture = 0; fixture < 16; ++fixture)
        {
            Prefix prefix{};
            for (std::size_t byte = 0; byte < prefix.size(); ++byte) prefix[byte] = std::uint8_t(byte * (fixture * 2 + 1) + 17 * fixture);
            Put(prefix, 0, 0x00700EA4); std::array<std::uint32_t, 4> old{}, arguments{};
            for (std::size_t index = 0; index < 4; ++index)
            {
                old[index] = words[(fixture + index * 3) % 16]; arguments[index] = words[(fixture * 5 + index * 7 + 1) % 16];
                Put(prefix, Offsets[index], old[index]);
            }
            if (fixture == 2 || fixture == 9) arguments = old;
            F::ComposedPropertiesForAnalysis result{}; require(F::ComposeRawPropertiesForAnalysis(prefix, arguments, result, Profile));
            auto state = prefix;
            for (std::size_t index = 0; index < 4; ++index)
            {
                require(result.rawGetterResults[index * 2] == old[index]); require(result.rawGetterResults[index * 2 + 1] == arguments[index]);
                Put(state, Offsets[index], arguments[index]); require(result.afterSet[index] == state);
                auto alias = index ? result.afterSet[index - 1] : prefix;
                require(F::WriteRawPropertyForAnalysis(alias, Properties[index], arguments[index], alias, Profile)); require(alias == state);
                require(F::WriteRawPropertyForAnalysis(alias, Properties[index], arguments[index], alias, Profile)); require(alias == state);
                require(ParseWord(WordHex(arguments[index])) == arguments[index]);
            }
        }
        Prefix prefix{}; Put(prefix, 0, 0x00700EA4); Prefix output{}; output.fill(0x5C); const auto unchanged = output;
        F::ComposedPropertiesForAnalysis composed{}; composed.rawGetterResults.fill(0xDEADBEEF); composed.afterSet.fill(output); const auto before = Render(composed);
        const std::array<std::uint32_t, 4> arguments{};
        require(!F::ComposeRawPropertiesForAnalysis(prefix, arguments, composed)); require(Render(composed) == before);
        require(!F::ComposeRawPropertiesForAnalysis(prefix, arguments, composed, static_cast<F::ProfileForAnalysis>(99))); require(Render(composed) == before);
        for (const auto property : Properties)
        {
            std::uint32_t raw = 0xDEADBEEF;
            require(!F::ReadRawPropertyForAnalysis(prefix, property, raw)); require(raw == 0xDEADBEEF);
            require(!F::WriteRawPropertyForAnalysis(prefix, property, 1, output)); require(output == unchanged);
        }
        std::uint32_t raw = 0xDEADBEEF;
        require(!F::ReadRawPropertyForAnalysis(prefix, static_cast<F::PropertyForAnalysis>(99), raw, Profile)); require(raw == 0xDEADBEEF);
        require(!F::WriteRawPropertyForAnalysis(prefix, static_cast<F::PropertyForAnalysis>(99), 1, output, Profile)); require(output == unchanged);
        prefix[0] ^= 1;
        require(!F::ComposeRawPropertiesForAnalysis(prefix, arguments, composed, Profile)); require(Render(composed) == before);
        for (const auto property : Properties)
        {
            require(!F::ReadRawPropertyForAnalysis(prefix, property, raw, Profile)); require(raw == 0xDEADBEEF);
            require(!F::WriteRawPropertyForAnalysis(prefix, property, 1, output, Profile)); require(output == unchanged);
        }
        std::cout << "CabinetPuzzleProperties: " << checks << " contracts passed\n";
        return 0;
    }
    catch (...) { return 1; }
}
