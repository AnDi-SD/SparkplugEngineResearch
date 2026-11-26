#include "Analysis/PC/wxTrollAttackStateAbi.h"
#include "Code/wxTrollAttackState.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using State = winx::reconstruction::wxTrollAttackState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;
    using ControllerPrefix = State::CapturedControllerPrefixForAnalysis;
    using Controller = State::BorrowedControllerForAnalysis;
    constexpr std::uint32_t ControllerAddress = 0x31000540;

    constexpr std::string_view InitialState =
        "20916f00000000000000cccc000000000300000000000000000000000101010101cccccc0000"
        "00000000000000cccccc00000000000000000000000000000000";
    constexpr std::string_view InitialController =
        "0c676f00000000000000cccc0000000000000000000000000000000000000000010000000000"
        "000000ccccccccccccccf0010031000000000101cccc4054094bf40100000000164300cccccc"
        "000000000000000000000000fc2f7000000000000000cccc0000000000000000000000000000"
        "0000000000000000000000000000000000000000000001ccccccffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f000000000000000000000000"
        "0000000000cccccc00000000000000000000003f000000000000003f00000000000000000000"
        "0000000000000000803f00000000cccccccc01cccccc03000000000000000000000000000000"
        "000000000000000001cccccc";

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    template<class UInt>
    UInt ParseUnsigned(std::string_view word)
    {
        UInt value{};
        const auto end = word.data() + word.size();
        const auto parsed = std::from_chars(word.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid hexadecimal capture");
        return value;
    }

    template<class Prefix>
    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        if (hex.size() != 2 * bytes.size() || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("unexpected hexadecimal capture width");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = std::uint8_t(ParseUnsigned<unsigned>(hex.substr(i * 2, 2)));
        return bytes;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (i * 8));
    }

    template<class Prefix>
    void PrintPrefix(const Prefix& bytes)
    {
        for (const auto value : bytes)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string target, stateHex, controllerHex, trailing;
            if (!(row >> target >> stateHex >> controllerHex) || row >> trailing || target.size() != 8 ||
                target.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("expected target and captured state with optional controller");
            const auto state = ParsePrefix<StatePrefix>(stateHex);
            std::optional<ControllerPrefix> controller;
            if (controllerHex != "-")
                controller = ParsePrefix<ControllerPrefix>(controllerHex);
            const Controller view = controller ? Controller{ControllerAddress, &*controller} : Controller{};
            const auto result = State::TryEvaluatePcCompletionForAnalysis(state, ParseUnsigned<std::uint32_t>(target), view);
            if (!result)
                throw std::runtime_error("Troll completion captures were refused");
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(*result) << ' ';
            PrintPrefix(state);
            std::cout << ' ';
            if (controller)
                PrintPrefix(*controller);
            else
                std::cout << '-';
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::uint32_t target, handle;
        std::uint32_t mark9, mark3, after9, after3;
        bool hasController;
        std::uint8_t al;
    };

    constexpr Witness Witnesses[]{
        {0x00000016u, 0x13572468u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, false, 1},
        {0x0000001cu, 0x80000011u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, false, 1},
        {0x0000001du, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, false, 1},
        {0x00000000u, 0x13572468u, 0x24681357u, 0x24681357u, 0x24681357u, 0x24681357u, true, 0},
        {0x00000000u, 0x13572468u, 0x13572468u, 0x13572468u, 0x00000000u, 0x00000000u, true, 1},
        {0x00000000u, 0x13572468u, 0x13572468u, 0x24681357u, 0x00000000u, 0x24681357u, true, 1},
        {0x00000000u, 0x13572468u, 0x24681357u, 0x13572468u, 0x24681357u, 0x00000000u, true, 1},
        {0x00000000u, 0x13572468u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, true, 0},
        {0x01000016u, 0x80000001u, 0x00000001u, 0x80000001u, 0x00000001u, 0x00000000u, true, 1},
        {0x8000001cu, 0x80000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, true, 0},
        {0xffffffffu, 0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, true, 1},
        {0x00000001u, 0x00000001u, 0x00000101u, 0x80000001u, 0x00000101u, 0x80000001u, true, 0},
    };

    void CheckWitnesses()
    {
        for (const auto& fixture : Witnesses)
        {
            auto state = ParsePrefix<StatePrefix>(InitialState);
            StoreWord(state.data() + Abi::wxTrollAttackStatePendingOffset, fixture.handle);
            StoreWord(state.data() + Abi::wxTrollAttackStateConsumerOffset,
                fixture.hasController ? ControllerAddress : 0xA5A5A5A5u);
            auto controller = ParsePrefix<ControllerPrefix>(InitialController);
            StoreWord(controller.data() + Abi::wxTrollAttackStateMark9Offset, fixture.mark9);
            StoreWord(controller.data() + Abi::wxTrollAttackStateMark3Offset, fixture.mark3);
            const auto beforeState = state;
            auto expected = controller;
            StoreWord(expected.data() + Abi::wxTrollAttackStateMark9Offset, fixture.after9);
            StoreWord(expected.data() + Abi::wxTrollAttackStateMark3Offset, fixture.after3);
            const Controller view = fixture.hasController ? Controller{ControllerAddress, &controller} : Controller{};
            const auto result = State::TryEvaluatePcCompletionForAnalysis(state, fixture.target, view);
            Require(result && *result == fixture.al, "unexpected Troll completion AL");
            Require(state == beforeState && controller == expected, "unexpected Troll completion capture effect");
        }
    }

    void CheckRejections()
    {
        auto state = ParsePrefix<StatePrefix>(InitialState);
        StoreWord(state.data() + Abi::wxTrollAttackStatePendingOffset, 0x13572468u);
        StoreWord(state.data() + Abi::wxTrollAttackStateConsumerOffset, ControllerAddress);
        auto controller = ParsePrefix<ControllerPrefix>(InitialController);
        StoreWord(controller.data() + Abi::wxTrollAttackStateMark3Offset, 0x13572468u);
        const auto prior = controller;
        const auto refuse = [&](const StatePrefix& input, const Controller& view)
        {
            Require(!State::TryEvaluatePcCompletionForAnalysis(input, 0, view) && controller == prior,
                "invalid Troll completion capture changed marks");
        };
        auto wrongState = state;
        wrongState[0] ^= 1;
        refuse(wrongState, {ControllerAddress, &controller});
        wrongState = state;
        StoreWord(wrongState.data() + Abi::wxTrollAttackStateConsumerOffset, 0);
        refuse(wrongState, {0, &controller});
        refuse(state, {ControllerAddress, nullptr});
        refuse(state, {ControllerAddress + 4, &controller});
        auto wrongController = controller;
        wrongController[0] ^= 1;
        const auto wrongPrior = wrongController;
        refuse(state, {ControllerAddress, &wrongController});
        Require(wrongController == wrongPrior, "wrong controller primary allowed a store");

        const Controller decoy{0xA5A5A5A5u, &wrongController};
        for (const auto target : {0x16u, 0x1Cu})
            Require(State::TryEvaluatePcCompletionForAnalysis(state, target, decoy) == std::uint8_t{1}
                && wrongController == wrongPrior, "direct target consulted the controller");
        StoreWord(state.data() + Abi::wxTrollAttackStatePendingOffset, 0);
        Require(State::TryEvaluatePcCompletionForAnalysis(state, 0, decoy) == std::uint8_t{1}
            && wrongController == wrongPrior, "zero pending handle consulted the controller");
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
        CheckRejections();
        std::cout << "TrollAttackStateCompletion: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
