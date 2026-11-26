#include "Analysis/PC/wxMovingStateAirEventsAbi.h"
#include "Code/wxTrollMovingState.h"
#include "Code/wxYetiMovingState.h"

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
    using winx::reconstruction::MovingStateAirEventCaptureForAnalysis;
    using winx::reconstruction::MovingStateAirEventNameForAnalysis;
    using winx::reconstruction::MovingStatePrefixForAnalysis;
    using winx::reconstruction::wxTrollMovingState;
    using winx::reconstruction::wxYetiMovingState;

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    void PutWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (8 * i));
    }

    template<std::size_t Count>
    std::array<std::uint8_t, Count> ParseHex(std::string_view text)
    {
        if (text.size() != 2 * Count)
            throw std::runtime_error("unexpected hexadecimal capture size");
        std::array<std::uint8_t, Count> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            const auto first = text.data() + 2 * i;
            unsigned value{};
            const auto parsed = std::from_chars(first, first + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != first + 2)
                throw std::runtime_error("invalid hexadecimal capture byte");
            bytes[i] = std::uint8_t(value);
        }
        return bytes;
    }

    bool Apply(std::string_view selector, MovingStatePrefixForAnalysis& state,
        const MovingStateAirEventCaptureForAnalysis& event)
    {
        if (selector == "troll")
            return wxTrollMovingState::TryApplyAirEventForAnalysis(state, event);
        if (selector == "yeti")
            return wxYetiMovingState::TryApplyAirEventForAnalysis(state, event);
        throw std::runtime_error("unknown moving state");
    }

    struct EventFixture
    {
        std::array<std::uint8_t, 0x20> argument{};
        std::array<std::uint8_t, 0x14> tag{};
        MovingStateAirEventNameForAnalysis name;
        std::uint32_t tagAddress = 0x31000800u;
        std::uint32_t nameAddress;

        EventFixture(const MovingStateAirEventNameForAnalysis& eventName, std::size_t index)
            : name(eventName), nameAddress(index % 2 == 0 ? 0x31000C01u : 0x31000C08u)
        {
            PutWord(argument.data() + 0x1C, tagAddress);
            PutWord(tag.data() + 0x10, nameAddress);
        }

        [[nodiscard]] MovingStateAirEventCaptureForAnalysis View() const noexcept
        {
            return {{argument.data(), argument.size()}, tagAddress,
                {tag.data(), tag.size()}, nameAddress, {name.data(), name.size()}};
        }
    };

    int Replay()
    {
        std::string line;
        std::size_t index{};
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string selector, stateHex, name, trailing;
            if (!(row >> selector >> stateHex >> name) || (row >> trailing))
                throw std::runtime_error("expected selector, state and name");

            auto state = ParseHex<0x40>(stateHex);
            const EventFixture event(ParseHex<16>(name), index);
            if (!Apply(selector, state, event.View()))
                throw std::runtime_error("moving state replay capture refused");
            for (const auto byte : state)
                std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
            std::cout << '\n';
            ++index;
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::string_view selector;
        std::string_view name;
        std::uint8_t old3C;
        std::uint8_t old3D;
        std::uint8_t expected;
    };

    constexpr Witness Witnesses[]{
        {"troll", "6576656e745f6169725f626567696e00", 0xB1, 0xA5, 0x01},
        {"troll", "6576656e745f6169725f626567696e00", 0xC2, 0x01, 0x01},
        {"troll", "6576656e745f6169725f656e6400a9f0", 0xD3, 0xFA, 0x00},
        {"troll", "6576656e745f6169725f656e6400ff80", 0xE4, 0x00, 0x00},
        {"troll", "6576656e745f6169725f626567696e58", 0xF5, 0x55, 0x55},
        {"troll", "6576656e745f6169725f656e645800ff", 0x06, 0x80, 0x80},
        {"troll", "4556454e545f4149525f424547494e00", 0x17, 0xFF, 0xFF},
        {"troll", "6576656e745f61697200626567696e00", 0x28, 0x01, 0x01},
        {"troll", "ffffffffffffffffffffffffffffffff", 0x39, 0xF0, 0xF0},
        {"troll", "00000000000000000000000000000000", 0x4A, 0x87, 0x87},
        {"yeti", "6576656e745f6169725f626567696e00", 0xA5, 0x80, 0x01},
        {"yeti", "6576656e745f6169725f626567696e00", 0x01, 0x91, 0x01},
        {"yeti", "6576656e745f6169725f656e6400a9f0", 0xFA, 0xA2, 0x00},
        {"yeti", "6576656e745f6169725f656e6400ff80", 0x00, 0xB3, 0x00},
        {"yeti", "6576656e745f6169725f626567696e58", 0x55, 0xC4, 0x55},
        {"yeti", "6576656e745f6169725f656e645800ff", 0x80, 0xD5, 0x80},
        {"yeti", "4556454e545f4149525f424547494e00", 0xFF, 0xE6, 0xFF},
        {"yeti", "6576656e745f61697200626567696e00", 0x01, 0xF7, 0x01},
        {"yeti", "ffffffffffffffffffffffffffffffff", 0xF0, 0x08, 0xF0},
        {"yeti", "00000000000000000000000000000000", 0x87, 0x19, 0x87},
    };

    void CheckWitnesses()
    {
        std::size_t index{};
        for (const auto& witness : Witnesses)
        {
            MovingStatePrefixForAnalysis state{};
            for (std::size_t i = 0; i < state.size(); ++i)
                state[i] = std::uint8_t(5 + 17 * index + 37 * i);
            const bool troll = witness.selector == "troll";
            PutWord(state.data(), troll ? Abi::wxTrollMovingStateVTable : Abi::wxYetiMovingStateVTable);
            state[0x3C] = witness.old3C;
            state[0x3D] = witness.old3D;
            auto expected = state;
            const auto offset = troll ? Abi::wxTrollMovingStateAirEventByteOffset : Abi::wxYetiMovingStateAirEventByteOffset;
            expected[offset] = witness.expected;
            const EventFixture event(ParseHex<16>(witness.name), index);
            Require(Apply(witness.selector, state, event.View()), "moving state witness refused");
            Require(state == expected, "moving state witness differs from the expected prefix");
            ++index;
        }
    }

    void CheckRefusals(std::string_view selector, std::uint32_t owner, std::uint32_t other)
    {
        const EventFixture event(ParseHex<16>("6576656e745f6169725f626567696e00"), 0);
        MovingStatePrefixForAnalysis state{};
        PutWord(state.data(), other);
        auto before = state;
        Require(!Apply(selector, state, event.View()) && state == before, "foreign primary accepted or changed");

        PutWord(state.data(), owner);
        before = state;
        auto malformed = event;
        malformed.argument[0x1C] ^= 1;
        Require(!Apply(selector, state, malformed.View()) && state == before, "incoherent tag link accepted or changed");

        malformed = event;
        malformed.tag[0x10] ^= 1;
        Require(!Apply(selector, state, malformed.View()) && state == before, "incoherent name link accepted or changed");

        malformed = event;
        malformed.tagAddress = 0;
        PutWord(malformed.argument.data() + 0x1C, 0);
        Require(!Apply(selector, state, malformed.View()) && state == before, "null tag address accepted or changed");

        malformed = event;
        malformed.nameAddress = 0;
        PutWord(malformed.tag.data() + 0x10, 0);
        Require(!Apply(selector, state, malformed.View()) && state == before, "null name address accepted or changed");

        auto shortView = event.View();
        shortView.argument.size = 0x1F;
        Require(!Apply(selector, state, shortView) && state == before, "short argument accepted or changed");

        shortView = event.View();
        shortView.tag.size = 0x13;
        Require(!Apply(selector, state, shortView) && state == before, "short tag accepted or changed");

        shortView = event.View();
        shortView.name.size = 15;
        Require(!Apply(selector, state, shortView) && state == before, "short name accepted or changed");

        auto nullView = event.View();
        nullView.argument.data = nullptr;
        Require(!Apply(selector, state, nullView) && state == before, "missing argument bytes accepted or changed");

        nullView = event.View();
        nullView.tag.data = nullptr;
        Require(!Apply(selector, state, nullView) && state == before, "missing tag bytes accepted or changed");

        nullView = event.View();
        nullView.name.data = nullptr;
        Require(!Apply(selector, state, nullView) && state == before, "missing name bytes accepted or changed");
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
        CheckWitnesses();
        CheckRefusals("troll", Abi::wxTrollMovingStateVTable, Abi::wxYetiMovingStateVTable);
        CheckRefusals("yeti", Abi::wxYetiMovingStateVTable, Abi::wxTrollMovingStateVTable);
        std::cout << "MovingStateAirEvents: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
