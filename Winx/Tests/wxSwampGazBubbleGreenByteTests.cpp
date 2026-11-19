#include "../Code/wxSwampGazBubble.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
namespace
{
using B = wxSwampGazBubble;
using Prefix = B::CapturedPrefixForAnalysis;
constexpr auto Profile = B::ProfileForAnalysis::PcCapturedPrimaryRawByte144;
void Put(Prefix& p, std::size_t at, std::uint32_t value)
{ for (unsigned b = 0; b < 4; ++b) p[at + b] = std::uint8_t(value >> (b * 8)); }
std::string Hex(const Prefix& p)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (const auto byte : p) out << std::setw(2) << unsigned(byte);
    return out.str();
}
std::string Byte(std::uint8_t value)
{ std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(2) << unsigned(value); return out.str(); }
std::uint32_t Word(const std::string& token)
{
    if (token.size() != 8 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("word");
    std::uint32_t value = 0;
    for (unsigned b = 0; b < 4; ++b) value |= std::uint32_t(std::stoul(token.substr(b * 2, 2), nullptr, 16)) << (b * 8);
    return value;
}
Prefix Parse(const std::string& token)
{
    Prefix p{};
    if (token.size() != p.size() * 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("prefix");
    for (std::size_t b = 0; b < p.size(); ++b) p[b] = std::uint8_t(std::stoul(token.substr(b * 2, 2), nullptr, 16));
    return p;
}
std::string Render(const B::ComposedGreenByteForAnalysis& v)
{ return Byte(v.beforeRaw) + " " + Hex(v.afterSet) + " " + Byte(v.afterRaw); }
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
                std::istringstream input(line); std::string prefix, argument, extra;
                if (!(input >> prefix >> argument) || input >> extra) return 2;
                B::ComposedGreenByteForAnalysis result{};
                if (!B::ComposeGreenLowByteForAnalysis(Parse(prefix), Word(argument), result, Profile)) return 3;
                std::cout << Render(result) << '\n';
            }
            return 0;
        }
        if (argc != 1) return 2;
        unsigned checks = 0;
        auto require = [&](bool ok) { if (!ok) throw std::runtime_error("contract"); ++checks; };
        constexpr std::uint32_t highs[] = {0u, 0x12345600u, 0xFFFFFF00u, 0x80000000u};
        for (unsigned byte = 0; byte < 256; ++byte)
        {
            Prefix before{};
            for (std::size_t i = 0; i < before.size(); ++i) before[i] = std::uint8_t(byte * 17 + i * 37);
            Put(before, 0, 0x006FE408); before[0x144] = std::uint8_t(byte ^ 0xA5);
            for (const auto high : highs)
            {
                auto expected = before; expected[0x144] = std::uint8_t(byte);
                B::ComposedGreenByteForAnalysis result{};
                require(B::ComposeGreenLowByteForAnalysis(before, high | byte, result, Profile));
                require(result.beforeRaw == std::uint8_t(byte ^ 0xA5) && result.afterRaw == byte && result.afterSet == expected);
                auto alias = before; require(B::WriteGreenLowByteForAnalysis(alias, high | byte, alias, Profile)); require(alias == expected);
                require(B::WriteGreenLowByteForAnalysis(alias, high | byte, alias, Profile)); require(alias == expected);
                // Existing RAW Damage profile remains independent and accepts arbitrary DWORD bits.
                auto damage = before; Put(damage, 0x14C, high | byte); Prefix oldOutput{};
                require(B::WriteDamageInflictedRawForAnalysis(before, high | byte, oldOutput, B::ProfileForAnalysis::PcCapturedPrimaryRawDword14C));
                require(oldOutput == damage);
            }
        }
        Prefix before{}; Put(before, 0, 0x006FE408); Prefix out{}; out.fill(0x5C); const auto untouched = out;
        std::uint8_t byte = 0xD7; B::ComposedGreenByteForAnalysis result{};
        result.beforeRaw = 0xD7; result.afterRaw = 0x6E; result.afterSet = out; const auto rendered = Render(result);
        for (auto profile : {B::ProfileForAnalysis::Unqualified, B::ProfileForAnalysis::PcCapturedPrimaryRawDword14C, static_cast<B::ProfileForAnalysis>(99)})
        {
            require(!B::ReadGreenByteForAnalysis(before, byte, profile)); require(byte == 0xD7);
            require(!B::WriteGreenLowByteForAnalysis(before, 1, out, profile)); require(out == untouched);
            require(!B::ComposeGreenLowByteForAnalysis(before, 1, result, profile)); require(Render(result) == rendered);
        }
        std::uint32_t damage = 0xDEADBEEF;
        require(!B::ReadDamageInflictedRawForAnalysis(before, damage, Profile)); require(damage == 0xDEADBEEF);
        require(!B::WriteDamageInflictedRawForAnalysis(before, 1, out, Profile)); require(out == untouched);
        before[0] ^= 1;
        require(!B::ReadGreenByteForAnalysis(before, byte, Profile)); require(byte == 0xD7);
        require(!B::WriteGreenLowByteForAnalysis(before, 1, out, Profile)); require(out == untouched);
        require(!B::ComposeGreenLowByteForAnalysis(before, 1, result, Profile)); require(Render(result) == rendered);
        std::cout << "SwampGazBubbleGreenByte: " << checks << " contracts passed\n";
        return 0;
    }
    catch (...) { return 1; }
}
