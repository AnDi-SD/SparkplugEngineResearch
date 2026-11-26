#include "Analysis/PC/wxDispelTriggerAbi.h"
#include "Code/wxDispelTrigger.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using Trigger = winx::reconstruction::wxDispelTrigger;
    using Prefix = Trigger::CapturedPrefixForAnalysis;

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix data{};
        if (hex.size() != data.size() * 2)
            throw std::runtime_error("expected a 452-byte hexadecimal trigger prefix");
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            unsigned value{};
            const auto first = hex.data() + i * 2;
            const auto parsed = std::from_chars(first, first + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != first + 2)
                throw std::runtime_error("invalid hexadecimal capture byte");
            data[i] = std::uint8_t(value);
        }
        return data;
    }

    bool Apply(std::string_view operation, Prefix& state)
    {
        if (operation == "set")
            return Trigger::TrySetPcActiveByteForAnalysis(state);
        if (operation == "clear")
            return Trigger::TryClearPcActiveByteForAnalysis(state);
        throw std::runtime_error("expected set or clear");
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string operation, prefix, extra;
            if (!(row >> operation >> prefix) || (row >> extra))
                throw std::runtime_error("expected operation and prefix");
            auto state = ParsePrefix(prefix);
            if (!Apply(operation, state))
                throw std::runtime_error("trigger capture was refused");
            for (const auto byte : state)
                std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    Prefix CapturedFixture(std::size_t index, std::uint8_t active)
    {
        Prefix state{};
        for (std::size_t i = 0; i < state.size(); ++i)
            state[i] = std::uint8_t(17 * index + 37 * i + 5);
        for (std::size_t i = 0; i < 4; ++i)
            state[i] = std::uint8_t(Abi::wxDispelTriggerVTable >> (8 * i));
        state[Abi::wxDispelTriggerActiveByteOffset] = active;
        return state;
    }

    void CheckRetainedChains()
    {
        constexpr std::uint8_t ActiveBytes[]{0, 1, 0x7F, 0x80, 0xFF, 2, 0xA5, 0x55};
        std::size_t index{};
        for (const auto byte : ActiveBytes)
        {
            auto state = CapturedFixture(index, byte);
            auto expected = state;
            const auto first = std::uint8_t(index % 2);
            for (const auto value : {first, std::uint8_t(1 - first)})
            {
                expected[Abi::wxDispelTriggerActiveByteOffset] = value;
                Require(Apply(value == 0 ? "clear" : "set", state), "captured trigger refused");
                Require(state == expected, "trigger store changed bytes outside the requested value");
            }
            ++index;
        }
    }

    void CheckPrimaryRefusals()
    {
        auto state = CapturedFixture(0, 0xA5);
        state[1] ^= 1;
        const auto before = state;
        Require(!Trigger::TrySetPcActiveByteForAnalysis(state) && state == before, "foreign set capture changed");
        Require(!Trigger::TryClearPcActiveByteForAnalysis(state) && state == before, "foreign clear capture changed");
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
        CheckRetainedChains();
        CheckPrimaryRefusals();
        std::cout << "DispelTriggerActiveByte: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
