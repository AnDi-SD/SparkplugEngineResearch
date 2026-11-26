#include "Analysis/PC/wxMissileStateAbi.h"
#include "Code/wxMissileState.h"

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
    using State = winx::reconstruction::wxMissileState;
    using Prefix = State::CapturedStatePrefixForAnalysis;
    namespace Abi = winx::evidence::pc;

    static_assert(Abi::wxMissileStateClassID == 0x29B0FF3A);
    static_assert(Abi::wxMissileStatePermissionSlotOffset == 0x34);
    static_assert(Abi::wxMissileStatePermissionByte == 0x00518780);
    static_assert(Abi::wxMissileStatePermissionByteOffset == 0x3C);

    struct Witness
    {
        std::string_view prefix;
        std::uint32_t ignored;
        std::uint8_t expected;
    };

    constexpr std::array<Witness, 10> Witnesses{{
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000000a15cd0", 0x00000000u, 0x00u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000001a25bd1", 0x0000000Au, 0x01u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000002a35ad2", 0x0000010Au, 0x02u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc0000000000000000000000007fa459d3", 0x80000000u, 0x7Fu},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000080a558d4", 0x00000001u, 0x80u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc000000000000000000000000ffa657d5", 0xFFFFFFFFu, 0xFFu},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000055a756d6", 0x01000000u, 0x55u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc000000000000000000000000a5a855d7", 0xFF00FF00u, 0xA5u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc00000000000000000000000000a954d8", 0x8000000Au, 0x00u},
        {"e08b6f00000000000000cccc0000000015000000000000000000000001010101"
         "01cccccc000000000000000000cccccc000000000000000000000000ffaa53d9", 0x0000000Au, 0xFFu},
    }};

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseWord(std::string_view raw, std::size_t width)
    {
        if (raw.size() != width || raw.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("invalid lowercase hex field");
        std::uint32_t result{};
        const auto end = raw.data() + raw.size();
        const auto parsed = std::from_chars(raw.data(), end, result, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid raw word");
        return result;
    }

    Prefix ParsePrefix(std::string_view raw)
    {
        Prefix result{};
        if (raw.size() != result.size() * 2)
            throw std::runtime_error("incorrect MissileState capture width");
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<std::uint8_t>(ParseWord(raw.substr(i * 2, 2), 2));
        return result;
    }

    std::uint8_t Evaluate(const Prefix& state, std::uint32_t rawArgument)
    {
        const auto before = state;
        const auto result = State::ReadPcPermissionByteForAnalysis(state, rawArgument);
        Require(result.has_value(), "typed MissileState capture refused");
        Require(state == before, "MissileState capture changed");
        return *result;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string rawState, rawArgument, trailing;
            if (!(fields >> rawState >> rawArgument) || fields >> trailing)
                throw std::runtime_error("expected one capture and one raw DWORD");
            const auto state = ParsePrefix(rawState);
            const auto al = Evaluate(state, ParseWord(rawArgument, 8));
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(al) << ' ';
            for (auto byte : state)
                std::cout << std::setw(2) << unsigned(byte);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    int RunTests()
    {
        for (const auto& row : Witnesses)
            Require(Evaluate(ParsePrefix(row.prefix), row.ignored) == row.expected, "MissileState raw AL differs");

        auto foreign = ParsePrefix(Witnesses[5].prefix);
        foreign[0] = 0x10;
        foreign[1] = 0x82; // The distinct wxBloomDyingState primary uses the same PC leaf.
        auto before = foreign;
        Require(!State::ReadPcPermissionByteForAnalysis(foreign, 0x0000000Au).has_value() && foreign == before,
            "foreign alias capture admitted");
        foreign = ParsePrefix(Witnesses[0].prefix);
        foreign[3] ^= 0x80;
        before = foreign;
        Require(!State::ReadPcPermissionByteForAnalysis(foreign, 0xFFFFFFFFu).has_value() && foreign == before,
            "malformed MissileState capture admitted");

        std::cout << "MissileStatePermissionByte: " << checks << " contracts passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunTests();
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
