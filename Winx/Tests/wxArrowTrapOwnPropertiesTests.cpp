#include "Analysis/PC/wxArrowTrapAbi.h"
#include "Code/wxArrowTrap.h"

#include <charconv>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Trap = winx::reconstruction::wxArrowTrap;
    using Receiver = Trap::CapturedPcPropertyReceiverForAnalysis;
    using Speed = Trap::PcSpeedReturnForAnalysis;
    using PairedSpeed = Trap::ComposedPcSpeedForAnalysis;
    using PairedPause = Trap::ComposedPcPauseForAnalysis;
    using Profile = Trap::PcPropertyProfileForAnalysis;
    namespace abi = winx::evidence::pc;
    constexpr auto Qualified = Profile::CapturedPrimary;
    unsigned checks = 0;

    void Require(bool value, const char* description)
    {
        if (!value) throw std::runtime_error(description);
        ++checks;
    }

    std::uint32_t ParseHex(std::string_view digits)
    {
        std::uint32_t value{};
        const auto end = digits.data() + digits.size();
        const auto parsed = std::from_chars(digits.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("expected hexadecimal input");
        return value;
    }

    template<std::size_t Width>
    std::array<std::uint8_t, Width> ParseBytes(std::string_view text)
    {
        if (text.size() != 2 * Width) throw std::runtime_error("wrong captured byte width");
        std::array<std::uint8_t, Width> result{};
        for (std::size_t byte = 0; byte != Width; ++byte)
            result[byte] = std::uint8_t(ParseHex(text.substr(2 * byte, 2)));
        return result;
    }

    template<std::size_t Width>
    std::string Hex(const std::array<std::uint8_t, Width>& bytes)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(2 * Width);
        for (const auto byte : bytes)
        {
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }
        return result;
    }

    void PutWord(Receiver& receiver, std::size_t offset, std::uint32_t word)
    {
        for (unsigned byte = 0; byte != 4; ++byte)
            receiver[offset + byte] = std::uint8_t(word >> (8 * byte));
    }

    Receiver Capture(unsigned row, std::uint8_t range, std::uint32_t speed, std::uint32_t pause)
    {
        Receiver receiver{};
        for (std::size_t i = 0; i != receiver.size(); ++i)
            receiver[i] = std::uint8_t(19 * row + 13 + 37 * (0x40 + i));
        PutWord(receiver, 0, abi::wxArrowTrapVtable);
        PutWord(receiver, 0x18, 0x31000400u);
        receiver[offsetof(abi::wxArrowTrapLayout, inRange)] = range;
        PutWord(receiver, abi::wxArrowTrapSpeedOffset, speed);
        PutWord(receiver, abi::wxArrowTrapPauseOffset, pause);
        return receiver;
    }

    void CheckRetainedCaptures()
    {
        struct Case
        {
            std::uint8_t range;
            std::uint32_t oldSpeed, newSpeed, oldPause, newPause;
            std::string_view beforeST0, afterST0;
        };
        constexpr Case cases[]{
            {1u, 0u, 0u, 0x000003E8u, 0x000003E8u, "00000000000000000000", "00000000000000000000"},
            {0u, 0x3F800000u, 0x80000000u, 0u, 1u, "0000000000000080ff3f", "00000000000000000080"},
            {0xA5u, 0x80000000u, 0u, 1u, 0u, "00000000000000000080", "00000000000000000000"},
            {1u, 0x3DCCCCCDu, 0xC0200000u, 0xFFFFFFFFu, 0xFFFFFFFEu, "0000000000cdccccfb3f", "00000000000000a000c0"},
            {0u, 0x00800000u, 0x7F7FFFFFu, 0x80000000u, 0x7FFFFFFFu, "0000000000000080813f", "0000000000ffffff7e40"},
            {1u, 0xFF7FFFFFu, 0x80800000u, 0x7FFFFFFFu, 0x80000000u, "0000000000ffffff7ec0", "000000000000008081bf"},
            {0xFFu, 0xBF800000u, 0xBF800000u, 0xA5A5A5A5u, 0xA5A5A5A5u, "0000000000000080ffbf", "0000000000000080ffbf"},
            {1u, 0x44FA0000u, 0x41200000u, 0x11112222u, 0xFFFFFFFFu, "00000000000000fa0940", "00000000000000a00240"},
        };
        unsigned index = 0;
        for (const auto& trial : cases)
        {
            const auto original = Capture(index++, trial.range, trial.oldSpeed, trial.oldPause);
            auto receiver = original;
            auto expected = original;
            PutWord(expected, abi::wxArrowTrapSpeedOffset, trial.newSpeed);
            const auto beforeST0 = ParseBytes<10>(trial.beforeST0);
            const auto afterST0 = ParseBytes<10>(trial.afterST0);
            Speed value;
            Require(Trap::TryReadPcSpeedForAnalysis(receiver, value, Qualified) && value == beforeST0,
                "speed getter did not preserve physical ST0");
            PairedSpeed speed;
            Require(Trap::TryComposePcSpeedForAnalysis(receiver, trial.newSpeed, speed, Qualified),
                "speed composition refused the planned input");
            Require(speed.before == beforeST0 && speed.after == afterST0 && speed.afterSet == expected,
                "speed composition differs from the complete expected capture");
            Require(receiver == original, "speed composition modified its input");
            Require(Trap::TryWritePcSpeedBitsForAnalysis(receiver, trial.newSpeed, receiver, Qualified)
                && receiver == expected, "aliased speed setter changed another byte");

            PutWord(expected, abi::wxArrowTrapPauseOffset, trial.newPause);
            const auto speedAfter = receiver;
            std::uint32_t raw{};
            Require(Trap::TryReadPcPauseForAnalysis(receiver, raw, Qualified) && raw == trial.oldPause,
                "pause getter did not return its full DWORD");
            PairedPause pause;
            Require(Trap::TryComposePcPauseForAnalysis(receiver, trial.newPause, pause, Qualified),
                "pause composition refused the planned input");
            Require(pause.before == trial.oldPause && pause.after == trial.newPause && pause.afterSet == expected,
                "pause composition differs from the complete expected capture");
            Require(receiver == speedAfter, "pause composition modified its input");
            Require(Trap::TryWritePcPauseForAnalysis(receiver, trial.newPause, receiver, Qualified)
                && receiver == expected, "aliased pause setter changed another byte");
        }
    }

    void CheckRefusalsAndRawSetters()
    {
        Receiver guard;
        guard.fill(0xBC);
        Speed speedSentinel;
        speedSentinel.fill(0xA5);
        const auto owner = Capture(0, 1, 0x3F800000u, 0x89ABCDEFu);
        auto other = owner;
        PutWord(other, 0, abi::wxArrowTrapVtable + 4u);
        const auto checkUnadmitted = [&](const Receiver& captured, Profile profile) {
            auto dst = guard;
            auto speed = speedSentinel;
            std::uint32_t pause = 0xABCD0123u;
            PairedSpeed speedPair{speedSentinel, guard, speedSentinel};
            PairedPause pausePair{pause, guard, pause};
            Require(!Trap::TryReadPcSpeedForAnalysis(captured, speed, profile)
                && speed == speedSentinel, "rejected speed getter changed output");
            Require(!Trap::TryWritePcSpeedBitsForAnalysis(captured, 0, dst, profile)
                && dst == guard, "rejected speed setter changed output");
            Require(!Trap::TryComposePcSpeedForAnalysis(captured, 0, speedPair, profile)
                && speedPair.before == speedSentinel && speedPair.after == speedSentinel && speedPair.afterSet == guard,
                "rejected speed composition changed output");
            Require(!Trap::TryReadPcPauseForAnalysis(captured, pause, profile)
                && pause == 0xABCD0123u, "rejected pause getter changed output");
            Require(!Trap::TryWritePcPauseForAnalysis(captured, 0, dst, profile)
                && dst == guard, "rejected pause setter changed output");
            Require(!Trap::TryComposePcPauseForAnalysis(captured, 0, pausePair, profile)
                && pausePair.before == pause && pausePair.after == pause && pausePair.afterSet == guard,
                "rejected pause composition changed output");
        };
        checkUnadmitted(owner, Profile::Unqualified);
        checkUnadmitted(other, Qualified);

        for (const auto bits : {0x00000001u, 0x807FFFFFu, 0x7F800000u, 0xFF800000u, 0x7FC12345u})
        {
            auto after = guard;
            auto expected = owner;
            PutWord(expected, abi::wxArrowTrapSpeedOffset, bits);
            Require(Trap::TryWritePcSpeedBitsForAnalysis(owner, bits, after, Qualified) && after == expected,
                "speed setter evaluated an unqualified float encoding");
            auto speed = speedSentinel;
            Require(!Trap::TryReadPcSpeedForAnalysis(after, speed, Qualified) && speed == speedSentinel,
                "speed getter accepted an out-of-profile encoding");
            PairedSpeed composed{speedSentinel, guard, speedSentinel};
            Require(!Trap::TryComposePcSpeedForAnalysis(owner, bits, composed, Qualified)
                && composed.before == speedSentinel && composed.after == speedSentinel && composed.afterSet == guard,
                "speed composition exposed a write when the final getter refused");
            Require(!Trap::TryComposePcSpeedForAnalysis(after, 0, composed, Qualified)
                && composed.before == speedSentinel && composed.after == speedSentinel && composed.afterSet == guard,
                "speed composition exposed a write when the initial getter refused");
            std::uint32_t pause{};
            Require(Trap::TryReadPcPauseForAnalysis(after, pause, Qualified) && pause == 0x89ABCDEFu,
                "pause getter inspected the speed field");
        }
    }

    void PrintWord(std::uint32_t raw)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(8) << raw;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string selector, prefixHex, argumentHex, extra;
            if (!(fields >> selector >> prefixHex >> argumentHex) || fields >> extra || argumentHex.size() != 8)
                throw std::runtime_error("expected a selector, complete capture and raw DWORD");
            const auto capture = ParseBytes<Receiver{}.size()>(prefixHex);
            const auto argument = ParseHex(argumentHex);
            if (selector == "speed")
            {
                PairedSpeed result;
                Require(Trap::TryComposePcSpeedForAnalysis(capture, argument, result, Qualified),
                    "speed replay input was refused");
                std::cout << Hex(result.before) << ' ' << Hex(result.afterSet) << ' ' << Hex(result.after) << '\n';
            }
            else if (selector == "pause")
            {
                PairedPause result;
                Require(Trap::TryComposePcPauseForAnalysis(capture, argument, result, Qualified),
                    "pause replay input was refused");
                PrintWord(result.before);
                std::cout << ' ' << Hex(result.afterSet) << ' ';
                PrintWord(result.after);
                std::cout << '\n';
            }
            else throw std::runtime_error("unknown ArrowTrap property");
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failed");
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay") return Replay();
        if (argc != 1) throw std::runtime_error("unexpected argument");
        CheckRetainedCaptures();
        CheckRefusalsAndRawSetters();
        std::cout << "ArrowTrap own property checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
