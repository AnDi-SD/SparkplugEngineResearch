#include "../Code/wxLever.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using B = wxLever;
using Prefix = B::CapturedPrefixForAnalysis;
constexpr auto Profile = B::ProfileForAnalysis::PcCapturedPrimaryActionRawDword124;
void Put(Prefix& bytes, std::size_t offset, std::uint32_t word)
{ for (unsigned byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(word >> (byte * 8)); }
std::string WordHex(std::uint32_t word)
{
    std::ostringstream output; output << std::hex << std::setfill('0');
    for (unsigned byte = 0; byte < 4; ++byte) output << std::setw(2) << unsigned(std::uint8_t(word >> (byte * 8)));
    return output.str();
}
Prefix Parse(const std::string& token)
{
    Prefix bytes{};
    if (token.size() != bytes.size() * 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) throw std::runtime_error("prefix");
    for (std::size_t byte = 0; byte < bytes.size(); ++byte) bytes[byte] = std::uint8_t(std::stoul(token.substr(byte * 2, 2), nullptr, 16));
    return bytes;
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
                std::istringstream input(line); std::string prefix, extra;
                if (!(input >> prefix) || input >> extra) return 2;
                std::uint32_t value = 0;
                if (!B::ReadActionTokenRawForAnalysis(Parse(prefix), value, Profile)) return 3;
                std::cout << WordHex(value) << '\n';
            }
            return 0;
        }
        if (argc != 1) return 2;
        unsigned checks = 0;
        auto require = [&](bool value) { if (!value) throw std::runtime_error("contract"); ++checks; };
        constexpr std::uint32_t Words[]{0u, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu,
            0x7F800000u, 0xFF800000u, 0x7FC00001u, 0x80000001u, 1u, 0x01020304u,
            0xA5A5A5A5u, 0x5A5A5A5Au, 0x12345678u, 0xFEDCBA98u, 0x00010000u, 0xDEADBEEFu};
        for (const auto word : Words)
            for (unsigned salt = 0; salt < 4; ++salt)
            {
                Prefix before{};
                for (std::size_t byte = 0; byte < before.size(); ++byte) before[byte] = std::uint8_t(salt * 17 + byte * 37);
                Put(before, 0, 0x00700910); Put(before, 0x124, word); const auto unchanged = before;
                std::uint32_t output = 0x5CD75CD7;
                require(B::ReadActionTokenRawForAnalysis(before, output, Profile));
                require(output == word); require(before == unchanged);
            }
        Prefix before{}; Put(before, 0, 0x00700910); const auto valid = before;
        std::uint32_t output = 0x5CD75CD7;
        for (const auto profile : {B::ProfileForAnalysis::Unqualified, static_cast<B::ProfileForAnalysis>(99)})
        {
            require(!B::ReadActionTokenRawForAnalysis(before, output, profile));
            require(output == 0x5CD75CD7); require(before == valid);
        }
        for (unsigned byte = 0; byte < 4; ++byte)
        {
            auto wrong = valid; wrong[byte] ^= 1; const auto unchanged = wrong;
            require(!B::ReadActionTokenRawForAnalysis(wrong, output, Profile));
            require(output == 0x5CD75CD7); require(wrong == unchanged);
        }
        std::cout << "LeverActionToken: " << checks << " contracts passed\n";
        return 0;
    }
    catch (...) { return 1; }
}
