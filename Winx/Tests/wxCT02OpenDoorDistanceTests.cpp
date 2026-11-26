#include "Analysis/PC/wxCT02OpenDoorAbi.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/wxCT02OpenDoor.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
    using Trigger = winx::reconstruction::wxCT02OpenDoor;
    using Point = Trigger::PointForAnalysis;
    using Words = std::array<std::uint32_t, 6>;
    namespace Abi = winx::evidence::pc;

    static_assert(Trigger::ClassID == Abi::CT02OpenDoorClassId);
    static_assert(std::is_same_v<Point, sparkplug::reconstruction::spNode::Vector3>);
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseHex(std::string_view text)
    {
        if (text.size() != 8)
            throw std::runtime_error("expected eight digits for a raw binary32 coordinate");
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, value, 16);
        if (result.ec != std::errc{} || result.ptr != end)
            throw std::runtime_error("expected hexadecimal coordinate");
        return value;
    }

    Point DecodePoint(const std::uint32_t* words)
    {
        Point point{};
        std::memcpy(point.data(), words, point.size() * sizeof(float));
        return point;
    }

    bool RunCase(const Words& words)
    {
        const auto first = DecodePoint(words.data());
        const auto second = DecodePoint(words.data() + 3);
        bool inside{};
        Require(Trigger::TryEvaluatePcProximityForAnalysis(first, second, inside), "qualified points were refused");
        Require(std::memcmp(first.data(), words.data(), first.size() * sizeof(float)) == 0
            && std::memcmp(second.data(), words.data() + 3, second.size() * sizeof(float)) == 0,
            "predicate changed a captured coordinate");
        return inside;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            Words words{};
            for (auto& word : words)
            {
                std::string value;
                if (!(fields >> value))
                    throw std::runtime_error("expected six captured coordinates");
                word = ParseHex(value);
            }
            std::string extra;
            if (fields >> extra)
                throw std::runtime_error("unexpected trailing replay field");
            std::cout << std::hex << std::setfill('0') << std::setw(8)
                      << static_cast<unsigned>(RunCase(words)) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckPredicate()
    {
        struct Case
        {
            Words words;
            bool inside;
        };
        constexpr std::array<Case, 12> cases{{
            {{0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x80000000, 0x80000000}, true},
            {{0x00000000, 0x00000000, 0x00000000, 0x42C80000, 0x00000000, 0x42C80000}, false},
            {{0x00000000, 0x00000000, 0x00000000, 0x430D0000, 0x00000000, 0x00000000}, true},
            {{0x00000000, 0x00000000, 0x00000000, 0x430E0000, 0x00000000, 0x00000000}, false},
            {{0x00000000, 0x00000000, 0x00000000, 0x42C80000, 0x42C60000, 0x00000000}, true},
            {{0x00000000, 0x00000000, 0x00000000, 0x42C80000, 0x42C80000, 0x00000000}, false},
            {{0x00000000, 0x00000000, 0x00000000, 0x42C80000, 0xC2C80000, 0x00000000}, false},
            {{0x00000000, 0x00000000, 0x00000000, 0x42C80000, 0xC2C60000, 0x00000000}, true},
            {{0x00000000, 0x26800000, 0x00000000, 0x42C80000, 0x42C80000, 0x00000000}, true},
            {{0x00000000, 0xA6800000, 0x00000000, 0x42C80000, 0x42C80000, 0x00000000}, false},
            {{0x26800000, 0x00000000, 0x00000000, 0x42C80000, 0x00000000, 0x42C80000}, true},
            {{0xA6800000, 0x00000000, 0x00000000, 0x42C80000, 0x00000000, 0x42C80000}, false},
        }};
        for (const auto& fixture : cases)
            Require(RunCase(fixture.words) == fixture.inside, "strict nearest/64 height and squared-distance comparison differs");
    }

    void CheckRefusals()
    {
        const auto verify = [](std::size_t lane, std::uint32_t invalid, bool previous)
        {
            Words words{};
            words[lane] = invalid;
            const auto first = DecodePoint(words.data());
            const auto second = DecodePoint(words.data() + 3);
            bool inside = previous;
            Require(!Trigger::TryEvaluatePcProximityForAnalysis(first, second, inside) && inside == previous,
                "unqualified points changed the output");
        };
        verify(0, 0x00000001, true);
        verify(1, 0x7F800000, false);
        verify(5, 0x7FC12345, true);
        verify(3, 0x46800001, false);
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
        CheckPredicate();
        CheckRefusals();
        std::cout << "CT02OpenDoorDistance: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
