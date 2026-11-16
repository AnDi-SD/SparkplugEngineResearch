#include "../Code/wxDamageProxy.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using D = wxDamageProxy;
constexpr auto Profile = D::ProfileForAnalysis::PcCapturedPrimaryRawDwords12412C;
void Put(D::CapturedPrefixForAnalysis& p, std::size_t at, std::uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte) p[at + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
}
std::string PrefixHex(const D::CapturedPrefixForAnalysis& p)
{
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (const auto byte : p) out << std::setw(2) << unsigned(byte);
    return out.str();
}
std::string WordHex(std::uint32_t word)
{
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(8) << word; return out.str();
}
D::CapturedPrefixForAnalysis ParsePrefix(const std::string& value)
{
    if (value.size() != 608 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("prefix");
    D::CapturedPrefixForAnalysis p{};
    for (std::size_t byte = 0; byte < p.size(); ++byte) p[byte] = std::uint8_t(std::stoul(value.substr(byte * 2, 2), nullptr, 16));
    return p;
}
std::uint32_t ParseWord(const std::string& value)
{
    if (value.size() != 8 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("word");
    return std::uint32_t(std::stoul(value, nullptr, 16));
}
std::string Render(const D::ComposedPropertiesForAnalysis& r)
{
    const auto& v = r.rawGetterResults;
    return WordHex(v[0]) + " " + WordHex(v[1]) + " " + PrefixHex(r.afterPeriodSet) + " " + WordHex(v[2]) + " " + WordHex(v[3])
        + " " + PrefixHex(r.afterPointsSet) + " " + WordHex(v[4]) + " " + WordHex(v[5]);
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
                std::istringstream in(line); std::string prefix, period, points, extra;
                if (!(in >> prefix >> period >> points) || (in >> extra)) return 2;
                const auto p = ParsePrefix(prefix); D::ComposedPropertiesForAnalysis r{};
                if (!D::ComposeRawPropertiesForAnalysis(p, ParseWord(period), ParseWord(points), r, Profile)) return 3;
                std::cout << Render(r) << '\n';
            }
            return 0;
        }
        if (argc != 1) return 2;
        unsigned checks = 0;
        auto require = [&](bool condition) { if (!condition) throw std::runtime_error("contract"); ++checks; };
        constexpr std::uint32_t words[] = {0, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x3F800000u, 0x7FC00000u, 0xDEADBEEFu, 0x01234567u};
        for (unsigned fixture = 0; fixture < 16; ++fixture)
        {
            D::CapturedPrefixForAnalysis p{};
            for (std::size_t byte = 0; byte < p.size(); ++byte) p[byte] = std::uint8_t(fixture * 17 + byte * (fixture < 8 ? 0 : 37));
            Put(p, 0, 0x006F7F68); const auto oldPeriod = words[fixture % 8], oldPoints = words[(fixture + 3) % 8];
            const auto newPeriod = words[(fixture + 5) % 8], newPoints = words[(fixture + 7) % 8];
            Put(p, 0x12C, oldPeriod); Put(p, 0x124, oldPoints);
            D::ComposedPropertiesForAnalysis r{}; require(D::ComposeRawPropertiesForAnalysis(p, newPeriod, newPoints, r, Profile));
            require(r.rawGetterResults == std::array<std::uint32_t, 6>{oldPeriod, oldPoints, newPeriod, oldPoints, newPeriod, newPoints});
            auto first = p; Put(first, 0x12C, newPeriod); auto last = first; Put(last, 0x124, newPoints);
            require(r.afterPeriodSet == first && r.afterPointsSet == last);
            auto alias = p; require(D::WriteRawPropertyForAnalysis(alias, D::PropertyForAnalysis::DamagePeriodRaw12C, newPeriod, alias, Profile));
            require(alias == first); require(D::WriteRawPropertyForAnalysis(alias, D::PropertyForAnalysis::DamagePeriodRaw12C, newPeriod, alias, Profile)); require(alias == first);
        }
        D::CapturedPrefixForAnalysis p{}; Put(p, 0, 0x006F7F68); auto output = p; output.fill(0x5C); const auto unchanged = output;
        require(!D::WriteRawPropertyForAnalysis(p, D::PropertyForAnalysis::DamagePeriodRaw12C, 1, output)); require(output == unchanged);
        require(!D::WriteRawPropertyForAnalysis(p, static_cast<D::PropertyForAnalysis>(99), 1, output, Profile)); require(output == unchanged);
        std::uint32_t raw = 0xDEADBEEF; require(!D::ReadRawPropertyForAnalysis(p, D::PropertyForAnalysis::DamagePointsRaw124, raw)); require(raw == 0xDEADBEEF);
        p[0] ^= 1; require(!D::WriteRawPropertyForAnalysis(p, D::PropertyForAnalysis::DamagePointsRaw124, 0, output, Profile)); require(output == unchanged);
        std::cout << "DamageProxyProperties: " << checks << " contracts passed\n";
        return 0;
    }
    catch (...) { return 1; }
}
