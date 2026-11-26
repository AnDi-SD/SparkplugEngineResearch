#include "Analysis/PC/wxShadowBeastAttackStateAbi.h"
#include "Analysis/PC/wxAnimationControllerAbi.h"
#include "Code/wxShadowBeastAttackState.h"

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
    using State = winx::reconstruction::wxShadowBeastAttackState;
    using StatePrefix = State::CapturedStatePrefixForAnalysis;
    using ControllerPrefix = State::CapturedControllerPrefixForAnalysis;
    using Controller = State::BorrowedControllerForAnalysis;
    constexpr std::uint32_t StateAddress = 0x31000040;
    constexpr std::uint32_t ControllerAddress = 0x31000540;
    constexpr auto Mark9 = offsetof(Abi::wxAnimationControllerLayout, mark9);
    constexpr auto Mark3 = offsetof(Abi::wxAnimationControllerLayout, mark3);

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseWord(std::string_view text)
    {
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid hexadecimal field");
        return value;
    }

    bool HasHexWidth(std::string_view text, std::size_t size)
    {
        return text.size() == size && text.find_first_not_of("0123456789abcdef") == std::string_view::npos;
    }

    template<class Prefix>
    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        if (!HasHexWidth(hex, 2 * bytes.size()))
            throw std::runtime_error("unexpected capture width or hexadecimal digit");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<std::uint8_t>(ParseWord(hex.substr(i * 2, 2)));
        return bytes;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < sizeof(word); ++i)
            bytes[i] = static_cast<std::uint8_t>(word >> (i * 8));
    }

    template<class Prefix>
    Prefix MakePrefix(unsigned item, unsigned salt, std::uint32_t primary)
    {
        Prefix bytes{};
        for (std::size_t offset = 0; offset < bytes.size(); ++offset)
            bytes[offset] = static_cast<std::uint8_t>(salt + item * 19u + offset * 37u);
        StoreWord(bytes.data(), primary);
        return bytes;
    }

    template<class Iterator>
    void PrintBytes(Iterator start, Iterator end)
    {
        for (; start != end; ++start)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(*start);
    }

    int Replay(bool archive)
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string target, stateHex, controllerHex, trailing;
            if (!(row >> target >> stateHex >> controllerHex) || row >> trailing || !HasHexWidth(target, 8))
                throw std::runtime_error("expected raw target, typed state and optional typed controller");
            const auto state = ParsePrefix<StatePrefix>(stateHex);
            std::optional<ControllerPrefix> controller;
            if (controllerHex != "-")
                controller = ParsePrefix<ControllerPrefix>(controllerHex);
            if (archive && !controller)
                throw std::runtime_error("archived mark projection requires a staged controller");
            const Controller view = controller ? Controller{ControllerAddress, &*controller} : Controller{};
            const auto result = State::TryEvaluatePcPermissionForAnalysis(state, ParseWord(target), StateAddress, view);
            if (!result)
                throw std::runtime_error("ShadowBeast permission captures were refused");

            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(*result) << ' ';
            if (archive)
                PrintBytes(controller->data() + Mark9, controller->data() + Mark3 + sizeof(std::uint32_t));
            else
            {
                PrintBytes(state.begin(), state.end());
                std::cout << ' ';
                if (controller)
                    PrintBytes(controller->begin(), controller->end());
                else
                    std::cout << '-';
            }
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::uint32_t target, handle, mark9, mark3, after9, after3;
        bool hasController;
        std::uint8_t al;
    };

    constexpr Witness Witnesses[]{
        {0x0000000au, 0x13572468u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, false, 1},
        {0x0000000au, 0x13572468u, 0x13572468u, 0x13572468u, 0x13572468u, 0x13572468u, true, 1},
        {0x00000000u, 0x00000000u, 0x8fe01001u, 0xfedcba98u, 0x8fe01001u, 0xfedcba98u, true, 1},
        {0x00000000u, 0x13572468u, 0x24681357u, 0x24681357u, 0x24681357u, 0x24681357u, true, 0},
        {0x00000000u, 0x13572468u, 0x13572468u, 0x13572468u, 0x00000000u, 0x00000000u, true, 1},
        {0x00000000u, 0x13572468u, 0x13572468u, 0x24681357u, 0x00000000u, 0x24681357u, true, 1},
        {0x00000000u, 0x13572468u, 0x24681357u, 0x13572468u, 0x24681357u, 0x00000000u, true, 1},
        {0x00000000u, 0x13572468u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, true, 0},
        {0x0000010au, 0x80000001u, 0x00000001u, 0x80000001u, 0x00000001u, 0x00000000u, true, 1},
        {0x8000000au, 0x80000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, true, 0},
        {0xffffffffu, 0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, true, 1},
        {0x00000001u, 0x00000001u, 0x00000101u, 0x80000001u, 0x00000101u, 0x80000001u, true, 0},
        {0x0000000bu, 0x11223344u, 0x11223345u, 0x00000000u, 0x11223345u, 0x00000000u, true, 0},
        {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, true, 1},
    };

    void CheckWitnesses()
    {
        unsigned index = 0;
        for (const auto& witness : Witnesses)
        {
            auto state = MakePrefix<StatePrefix>(index, 13, Abi::wxShadowBeastAttackStateVTable);
            auto controller = MakePrefix<ControllerPrefix>(index, 113, Abi::wxAnimationControllerVtable);
            ++index;
            StoreWord(state.data() + Abi::wxShadowBeastAttackStatePendingOffset, witness.handle);
            StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset,
                witness.hasController ? ControllerAddress : 0xA5A5A5A5u);
            StoreWord(controller.data() + Mark9, witness.mark9);
            StoreWord(controller.data() + Mark3, witness.mark3);
            const auto beforeState = state;
            auto expected = controller;
            StoreWord(expected.data() + Mark9, witness.after9);
            StoreWord(expected.data() + Mark3, witness.after3);
            const Controller view = witness.hasController ? Controller{ControllerAddress, &controller} : Controller{};
            const auto result = State::TryEvaluatePcPermissionForAnalysis(state, witness.target, StateAddress, view);
            Require(result && *result == witness.al, "unexpected ShadowBeast permission AL");
            Require(state == beforeState, "unexpected ShadowBeast state mutation");
            Require(controller == expected, "unexpected borrowed controller mutation");
        }
    }

    void CheckBindings()
    {
        auto state = MakePrefix<StatePrefix>(2, 13, Abi::wxShadowBeastAttackStateVTable);
        auto controller = MakePrefix<ControllerPrefix>(2, 113, Abi::wxAnimationControllerVtable);
        constexpr std::uint32_t handle = 0x13572468;
        StoreWord(controller.data() + Mark9, handle);
        StoreWord(controller.data() + Mark3, handle);
        const auto prior = controller;

        const auto refuse = [&](std::uint32_t target, std::uint32_t owner, const Controller& view)
        {
            const auto stateBefore = state;
            const auto controllerBefore = view.prefix ? *view.prefix : prior;
            Require(!State::TryEvaluatePcPermissionForAnalysis(state, target, owner, view)
                && state == stateBefore && controller == prior && (!view.prefix || *view.prefix == controllerBefore),
                "invalid ShadowBeast capture produced a result or changed bytes");
        };
        StoreWord(state.data() + Abi::wxShadowBeastAttackStatePendingOffset, handle);
        StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset, ControllerAddress);
        state[0] ^= 1;
        refuse(0, StateAddress, {ControllerAddress, &controller});
        refuse(10, StateAddress, {ControllerAddress, &controller});
        state[0] ^= 1;

        refuse(0, StateAddress, {ControllerAddress, nullptr});
        refuse(0, StateAddress, {ControllerAddress + 4, &controller});
        refuse(0, 0, {ControllerAddress, &controller});
        refuse(0, ControllerAddress + 4, {ControllerAddress, &controller});
        refuse(0, 0xFFFFFFFCu, {ControllerAddress, &controller});
        auto wrongPrimary = controller;
        wrongPrimary[0] ^= 1;
        refuse(0, StateAddress, {ControllerAddress, &wrongPrimary});
        StoreWord(state.data() + Abi::wxShadowBeastAttackStatePendingOffset, 0);
        refuse(0, StateAddress, {ControllerAddress, &wrongPrimary});
        refuse(0, StateAddress, {});
        StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset, 0);
        refuse(0, StateAddress, {0, &controller});
        StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset, StateAddress + 4);
        refuse(0, StateAddress, {StateAddress + 4, &controller});
        constexpr auto topAddress = 0xFFFFFFFFu;
        StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset, topAddress);
        refuse(0, StateAddress, {topAddress, &controller});

        const auto before = state;
        const auto wrongBefore = wrongPrimary;
        Require(State::TryEvaluatePcPermissionForAnalysis(state, 10, 0, {0, &wrongPrimary}) == std::uint8_t{1}
            && state == before && wrongPrimary == wrongBefore && controller == prior, "mode 10 consulted forwarded capture");
        StoreWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset, StateAddress + static_cast<std::uint32_t>(state.size()));
        Require(State::TryEvaluatePcPermissionForAnalysis(state, 0, StateAddress,
            {StateAddress + static_cast<std::uint32_t>(state.size()), &controller}) == std::uint8_t{1} && controller == prior,
            "adjacent detached range was refused or zero handle changed marks");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay(false);
        if (argc == 2 && std::string_view(argv[1]) == "--replay-archived")
            return Replay(true);
        if (argc != 1)
            return 2;
        CheckWitnesses();
        CheckBindings();
        std::cout << "ShadowBeastAttackStatePermission: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
