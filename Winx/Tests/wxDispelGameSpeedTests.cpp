#include "../Code/wxDispelGame.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
using B = wxDispelGame;
constexpr auto Qualified = B::ProfileForAnalysis::PcPristineReadOnlyGlobal006DBA9CZero;
constexpr auto Completed = B::ResultForAnalysis::Completed;
unsigned contracts = 0;

void Require(bool condition)
{
    if (!condition) throw std::runtime_error("DispelGameSpeed contract failed");
    ++contracts;
}

std::array<B::ScalarReturnForAnalysis, 3> Compose(const std::uint32_t argument)
{
    std::array<B::ScalarReturnForAnalysis, 3> values{};
    Require(B::ReadGameSpeedForAnalysis(values[0], Qualified) == Completed);
    Require(B::WriteGameSpeedBitsForAnalysis(argument, Qualified) == Completed);
    Require(B::ReadGameSpeedForAnalysis(values[1], Qualified) == Completed);
    Require(B::WriteGameSpeedBitsForAnalysis(argument, Qualified) == Completed);
    Require(B::ReadGameSpeedForAnalysis(values[2], Qualified) == Completed);
    return values;
}

void Print(const B::ScalarReturnForAnalysis& value)
{
    for (const auto byte : value)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
}

void Unit()
{
    B::ScalarReturnForAnalysis output{};
    output.fill(0xA5);
    const auto before = output;
    Require(B::ReadGameSpeedForAnalysis(output) == B::ResultForAnalysis::UnqualifiedProfile);
    Require(output == before);
    Require(B::WriteGameSpeedBitsForAnalysis(0xFFFFFFFFu) == B::ResultForAnalysis::UnqualifiedProfile);
    for (const auto invalid : {B::ProfileForAnalysis::Unqualified,
        static_cast<B::ProfileForAnalysis>(255)})
    {
        Require(B::ReadGameSpeedForAnalysis(output, invalid) == B::ResultForAnalysis::UnqualifiedProfile);
        Require(output == before);
        Require(B::WriteGameSpeedBitsForAnalysis(0x7F800001u, invalid) == B::ResultForAnalysis::UnqualifiedProfile);
    }
    constexpr std::array<std::uint32_t, 16> arguments{0, 1, 0xFFFFFFFFu, 0x80000000u,
        0x7FFFFFFFu, 0x3F800000u, 0xBF800000u, 0x7F800000u, 0xFF800000u,
        0x7FC00001u, 0x7F800001u, 0x00800000u, 0x007FFFFFu, 0x80800000u,
        0xA5A5A5A5u, 0x5A5A5A5Au};
    for (const auto argument : arguments)
    {
        const auto values = Compose(argument);
        for (const auto& value : values) Require(value == B::ScalarReturnForAnalysis{});
    }
    // Output is ten physical little-endian x87 payload bytes, including +zero sign.
    Require(B::ReadGameSpeedForAnalysis(output, Qualified) == Completed);
    Require(output == B::ScalarReturnForAnalysis{});
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay")
        {
            std::string input;
            while (std::getline(std::cin, input))
            {
                if (input.size() != 8 || input.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                    throw std::runtime_error("Expected exactly eight hexadecimal argument digits");
                const auto argument = static_cast<std::uint32_t>(std::stoull(input, nullptr, 16));
                const auto values = Compose(argument);
                Print(values[0]); std::cout << ' '; Print(values[1]); std::cout << ' '; Print(values[2]);
                std::cout << '\n';
            }
            return 0;
        }
        if (argc != 1) throw std::runtime_error("Unknown DispelGameSpeed argument");
        Unit();
        std::cout << "DispelGameSpeed: " << contracts << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
