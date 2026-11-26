#include "Analysis/PC/spDXGamepadAbi.h"
#include "Code/SparkplugDX/spDXGamepad.h"
#include "Code/SparkplugPC/spDXInputDevice.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Gamepad = sparkplug::reconstruction::spDXGamepad;
    using Prefix = Gamepad::CapturedPcPrefixForAnalysis;
    namespace Abi = sparkplug::evidence::pc;

    static_assert(Gamepad::ClassID == Abi::DXGamepadClassId);
    static_assert(sparkplug::reconstruction::spDXInputDevice::ClassID == Abi::DXGamepadRegistrationBase);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseHex(std::string_view text, std::size_t width)
    {
        if (text.size() != width)
            throw std::runtime_error("unexpected hexadecimal width");
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, value, 16);
        if (result.ec != std::errc{} || result.ptr != end)
            throw std::runtime_error("expected hexadecimal input");
        return value;
    }

    Prefix ParsePrefix(std::string_view text)
    {
        Prefix prefix{};
        if (text.size() != 2 * prefix.size())
            throw std::runtime_error("expected a 192-byte cached gamepad prefix");
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = std::uint8_t(ParseHex(text.substr(2 * i, 2), 2));
        return prefix;
    }

    void PutWord(std::uint8_t* bytes, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(value >> (8 * i));
    }

    Prefix MakePrefix()
    {
        Prefix prefix{};
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = std::uint8_t(37 * i + 5);
        PutWord(prefix.data(), 0x006F30B4);
        PutWord(prefix.data() + 0x14, 0x006F3098);
        prefix[0x44] = 0xFF;
        prefix[0x45] = 0;
        PutWord(prefix.data() + 0x90, 0xF1234567);
        PutWord(prefix.data() + 0x94, 0x80000000);
        PutWord(prefix.data() + 0xA0, 0x80000001);
        PutWord(prefix.data() + 0xA4, 0xFFFFFFFF);
        const std::array<std::uint8_t, 16> buttons{
            0x00, 0x01, 0x02, 0x80, 0xFF, 0x00, 0x40, 0x00,
            0x00, 0x03, 0x00, 0x01, 0x80, 0x10, 0x00, 0x02,
        };
        for (std::size_t i = 0; i < buttons.size(); ++i)
            prefix[0xB0 + i] = buttons[i];
        return prefix;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string prefixHex, selectorHex, extra;
            if (!(fields >> prefixHex >> selectorHex) || fields >> extra)
                throw std::runtime_error("expected captured cache and selector");
            const auto prefix = ParsePrefix(prefixHex);
            std::uint32_t word{};
            Require(Gamepad::TryReadPcCachedValueForAnalysis(prefix, ParseHex(selectorHex, 8), word),
                "qualified replay was refused");
            std::cout << std::hex << std::setfill('0') << std::setw(8) << word << ' ';
            for (const auto byte : prefix)
                std::cout << std::setw(2) << static_cast<unsigned>(byte);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckQualified()
    {
        auto prefix = MakePrefix();
        const auto verify = [&](std::uint32_t selector, std::uint32_t expected)
        {
            const auto before = prefix;
            std::uint32_t word = ~expected;
            Require(Gamepad::TryReadPcCachedValueForAnalysis(prefix, selector, word), "qualified cache was refused");
            Require(word == expected, "cached result differs from the full native EAX word");
            Require(prefix == before, "query changed the captured cache");
        };
        const std::array<std::uint32_t, 32> expected{
            0xF1234567, 0x80000000, 0x80000001, 0x00000001,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 0, 1, 0,
            0, 1, 0, 1, 1, 1, 0, 1,
        };
        for (std::uint32_t i = 0; i < expected.size(); ++i)
            verify(111u + i, expected[i]);
        for (const auto selector : {0u, 110u, 143u, 0xFFFFFFFFu})
            verify(selector, 0);

        prefix[0x44] = 0;
        prefix[0x45] = 0xFF;
        verify(111, 0);
        verify(128, 0);
    }

    void CheckRefusals()
    {
        const auto verify = [](const Prefix& prefix)
        {
            const auto before = prefix;
            std::uint32_t word = 0xABCD1234;
            Require(!Gamepad::TryReadPcCachedValueForAnalysis(prefix, 111, word) && word == 0xABCD1234,
                "foreign identity changed output");
            Require(prefix == before, "refusal changed the captured cache");
        };
        auto wrongOwner = MakePrefix();
        PutWord(wrongOwner.data(), 0x006F3098);
        verify(wrongOwner);
        auto wrongInterface = MakePrefix();
        PutWord(wrongInterface.data() + 0x14, 0x006F30B4);
        verify(wrongInterface);
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
        CheckQualified();
        CheckRefusals();
        std::cout << "DXGamepadCachedValue: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
