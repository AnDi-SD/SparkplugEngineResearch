#include "Analysis/PC/spDXKeyboardAbi.h"
#include "Code/SparkplugDX/spDXKeyboard.h"
#include "Code/SparkplugPC/spDXInputDevice.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Keyboard = sparkplug::reconstruction::spDXKeyboard;
    using Prefix = Keyboard::CapturedPcPrefixForAnalysis;
    namespace Abi = sparkplug::evidence::pc;

    static_assert(Keyboard::ClassID == Abi::DXKeyboardClassId && Abi::DXKeyboardChangedSlot == 0x08
        && Abi::DXKeyboardChangedQuery == 0x004CCA00);
    static_assert(sparkplug::reconstruction::spDXInputDevice::ClassID == Abi::DXKeyboardRegistrationBase);

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
        const auto parsed = std::from_chars(text.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("expected a hexadecimal word");
        return value;
    }

    Prefix ParsePrefix(std::string_view text)
    {
        Prefix prefix{};
        if (text.size() != 2 * prefix.size())
            throw std::runtime_error("expected a 2604-byte keyboard prefix");
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
        PutWord(prefix.data(), 0x006F31D4);
        PutWord(prefix.data() + 0x14, 0x006F31B8);
        prefix[0x44] = 0;
        prefix[0x45] = 0xFF;
        std::fill_n(prefix.begin() + Abi::DXKeyboardChangedCacheOffset,
            prefix.size() - Abi::DXKeyboardChangedCacheOffset, std::uint8_t(2));
        return prefix;
    }

    std::uint32_t ExpectedScan(std::uint32_t key)
    {
        constexpr std::array<std::uint8_t, 18> extended{
            87, 88, 156, 157, 181, 183, 184, 197, 199, 200, 201, 203, 205, 207, 208, 209, 210, 211,
        };
        if (key < 40)
            return key + 1;
        if (key < 82)
            return key + 2;
        if (key < 100)
            return extended[key - 82];
        return key;
    }

    void Verify(const Prefix& prefix, std::uint32_t key, std::uint8_t expected)
    {
        const auto before = prefix;
        std::uint8_t output = 0xAB;
        Require(Keyboard::TryReadPcChangedKeyForAnalysis(prefix, key, output)
            && output == expected && prefix == before, "changed AL contract differs");
    }

    void CheckKnownMap()
    {
        for (std::uint32_t key = 0; key <= 176; ++key)
        {
            if (key >= 143 && key != 176)
                continue;
            auto prefix = MakePrefix();
            for (const auto value : {0u, 1u, 2u, 0x80u, 0xFFu})
            {
                prefix[Abi::DXKeyboardChangedCacheOffset + ExpectedScan(key)] = std::uint8_t(value);
                Verify(prefix, key, std::uint8_t(value == 1));
            }
        }
    }

    void CheckFixtures()
    {
        struct Fixture
        {
            std::uint32_t key;
            std::uint8_t acquired, field44, current, changed, changedDecoy, expectedAL;
        };
        constexpr Fixture fixtures[]{
            {0, 1, 0, 0, 1, 0, 1},
            {0, 2, 1, 1, 2, 1, 0},
            {70, 1, 1, 1, 0, 1, 0},
            {99, 128, 0, 0, 1, 0, 1},
            {99, 255, 1, 1, 255, 1, 0},
            {142, 1, 1, 0, 1, 0, 1},
            {176, 1, 0, 0, 1, 0, 1},
            {176, 0, 1, 1, 1, 1, 0}
        };
        for (const auto& fixture : fixtures)
        {
            auto prefix = MakePrefix();
            const auto scan = ExpectedScan(fixture.key);
            prefix[0x44] = fixture.field44;
            prefix[Abi::DXKeyboardAcquiredOffset] = fixture.acquired;
            std::fill_n(prefix.begin() + Abi::DXKeyboardChangedCacheOffset,
                prefix.size() - Abi::DXKeyboardChangedCacheOffset, fixture.changedDecoy);
            prefix[Abi::DXKeyboardCurrentCacheOffset + scan] = fixture.current;
            prefix[Abi::DXKeyboardChangedCacheOffset + scan] = fixture.changed;
            Verify(prefix, fixture.key, fixture.expectedAL);
            std::uint8_t currentResult = 0xAB;
            Require(Keyboard::TryReadPcCurrentKeyForAnalysis(prefix, fixture.key, currentResult)
                && currentResult == std::uint8_t(fixture.acquired != 0 && fixture.current == 1),
                "current query no longer reads the current cache");
        }
    }

    void CheckRefusals()
    {
        const auto refuse = [](const Prefix& prefix, std::uint32_t key)
        {
            const auto before = prefix;
            std::uint8_t output = 0xAB;
            Require(!Keyboard::TryReadPcChangedKeyForAnalysis(prefix, key, output)
                && output == 0xAB && prefix == before, "refusal changed output or captured prefix");
        };
        auto prefix = MakePrefix();
        for (const auto acquired : {0u, 1u})
        {
            prefix[0x45] = std::uint8_t(acquired);
            for (const auto key : {143u, 144u, 175u, 177u, 0x010000B0u, 0xFFFFFFFFu})
                refuse(prefix, key);
        }
        auto wrongPrimary = prefix;
        wrongPrimary[0] ^= 1;
        refuse(wrongPrimary, 0);
        auto wrongInterface = prefix;
        wrongInterface[0x14] ^= 1;
        refuse(wrongInterface, 176);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string prefixHex, keyHex, extra;
            if (!(fields >> prefixHex >> keyHex) || fields >> extra)
                throw std::runtime_error("expected captured keyboard prefix and raw key");
            const auto prefix = ParsePrefix(prefixHex);
            std::uint8_t al{};
            Require(Keyboard::TryReadPcChangedKeyForAnalysis(prefix, ParseHex(keyHex, 8), al),
                "qualified replay row was refused");
            std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(al) << ' ';
            for (const auto byte : prefix)
                std::cout << std::setw(2) << static_cast<unsigned>(byte);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
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
        CheckKnownMap();
        CheckFixtures();
        CheckRefusals();
        std::cout << "DXKeyboardChangedCache: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
