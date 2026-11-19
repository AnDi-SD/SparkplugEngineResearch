#include "../Code/wxQuietusCarnivorous.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using B = wxQuietusCarnivorous;
using Prefix = B::CapturedPrefixForAnalysis;
constexpr auto Profile = B::ProfileForAnalysis::PcCapturedPrimaryRawDword164;
void Put(Prefix& p, std::size_t at, std::uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte) p[at + byte] = std::uint8_t(value >> (byte * 8));
}
std::string Hex(const Prefix& p)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (const auto byte : p) out << std::setw(2) << unsigned(byte);
    return out.str();
}
std::string Word(std::uint32_t value)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (unsigned byte = 0; byte < 4; ++byte) out << std::setw(2) << unsigned(std::uint8_t(value >> (byte * 8)));
    return out.str();
}
std::uint32_t ParseWord(const std::string& token)
{
    if (token.size() != 8 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("word");
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) value |= std::uint32_t(std::stoul(token.substr(byte * 2, 2), nullptr, 16)) << (byte * 8);
    return value;
}
Prefix ParsePrefix(const std::string& token)
{
    Prefix p{};
    if (token.size() != p.size() * 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("prefix");
    for (std::size_t byte = 0; byte < p.size(); ++byte) p[byte] = std::uint8_t(std::stoul(token.substr(byte * 2, 2), nullptr, 16));
    return p;
}
std::string Render(const B::ComposedDamageForAnalysis& value)
{
    return Word(value.beforeRaw) + " " + Hex(value.afterSet) + " " + Word(value.afterRaw);
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
                std::istringstream input(line); std::string prefix, argument, extra;
                if (!(input >> prefix >> argument) || (input >> extra)) return 2;
                B::ComposedDamageForAnalysis result{};
                if (!B::ComposeDamageRawForAnalysis(ParsePrefix(prefix), ParseWord(argument), result, Profile)) return 3;
                std::cout << Render(result) << '\n';
            }
            return 0;
        }
        if (argc != 1) return 2;
        unsigned checks = 0;
        auto require = [&](bool ok) { if (!ok) throw std::runtime_error("contract"); ++checks; };
        constexpr std::uint32_t words[] = {0, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x7F800000u, 0xFF800000u,
            0x7FC00001u, 0x80000001u, 1, 0x01020304u, 0xA5A5A5A5u, 0x5A5A5A5Au, 0x12345678u, 0xFEDCBA98u, 0x00010000u, 0xDEADBEEFu};
        for (unsigned fixture = 0; fixture < 16; ++fixture)
        {
            Prefix before{};
            for (std::size_t byte = 0; byte < before.size(); ++byte) before[byte] = std::uint8_t(fixture * 17 + (fixture < 8 ? 0 : byte * 37));
            Put(before, 0, 0x006FFD30); const auto old = words[fixture];
            const auto argument = (fixture == 2 || fixture == 9) ? old : words[(fixture * 5 + 1) % 16];
            Put(before, 0x164, old); auto after = before; Put(after, 0x164, argument);
            B::ComposedDamageForAnalysis result{}; require(B::ComposeDamageRawForAnalysis(before, argument, result, Profile));
            require(result.beforeRaw == old && result.afterRaw == argument && result.afterSet == after);
            auto alias = before; require(B::WriteDamageRawForAnalysis(alias, argument, alias, Profile)); require(alias == after);
            require(B::WriteDamageRawForAnalysis(alias, argument, alias, Profile)); require(alias == after);
            require(ParseWord(Word(argument)) == argument);
        }
        Prefix before{}; Put(before, 0, 0x006FFD30); Prefix out{}; out.fill(0x5C); const auto untouched = out;
        std::uint32_t raw = 0xDEADBEEF; B::ComposedDamageForAnalysis result{}; result.beforeRaw = result.afterRaw = 0xDEADBEEF; result.afterSet = out;
        const auto rendered = Render(result);
        for (auto profile : {B::ProfileForAnalysis::Unqualified, static_cast<B::ProfileForAnalysis>(99)})
        {
            require(!B::ReadDamageRawForAnalysis(before, raw, profile)); require(raw == 0xDEADBEEF);
            require(!B::WriteDamageRawForAnalysis(before, 1, out, profile)); require(out == untouched);
            require(!B::ComposeDamageRawForAnalysis(before, 1, result, profile)); require(Render(result) == rendered);
        }
        before[0] ^= 1;
        require(!B::ReadDamageRawForAnalysis(before, raw, Profile)); require(raw == 0xDEADBEEF);
        require(!B::WriteDamageRawForAnalysis(before, 1, out, Profile)); require(out == untouched);
        require(!B::ComposeDamageRawForAnalysis(before, 1, result, Profile)); require(Render(result) == rendered);
        std::cout << "QuietusCarnivorousDamage: " << checks << " contracts passed\n";
        return 0;
    }
    catch (...) { return 1; }
}
