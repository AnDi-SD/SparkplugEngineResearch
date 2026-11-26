#include "Analysis/PC/wxPivotingDoorAbi.h"
#include "Code/wxPivotingDoor.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Door = winx::reconstruction::wxPivotingDoor;
    using DoorCapture = Door::CapturedDoorPrefixForAnalysis;
    using NodeCapture = Door::BorrowedPlayerNodePrefixForAnalysis;
    using Words = std::array<std::uint32_t, 7>;
    namespace Abi = winx::evidence::pc;

    static_assert(Abi::wxPivotingDoorClassID == 0x4AC3694A);
    static_assert(Abi::wxPivotingDoorInteractionSlot == 0x4C);
    static_assert(Abi::wxPivotingDoorCachedDistanceEntry == 0x00539C90);

    constexpr std::array<std::size_t, 3> DoorPosition{0x190, 0x194, 0x198};
    constexpr std::array<std::size_t, 3> NodePosition{0x74, 0x78, 0x7C};

    struct Witness
    {
        Words words;
        bool expected;
    };

    constexpr std::array<Witness, 14> Witnesses{{
        // archived-inclusion
        {{0x40400000u, 0x4347C000u, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00000019u}, true},
        // archived-below-threshold
        {{0x40400000u, 0x4347C000u, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00000018u}, false},
        // include-plus-200
        {{0x40400000u, 0x43480000u, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00009C59u}, true},
        // include-minus-200
        {{0x40400000u, 0xC3480000u, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00009C58u}, false},
        // just-below-height
        {{0x40400000u, 0x4347FFFFu, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00000019u}, true},
        // just-above-height-negative
        {{0x40400000u, 0xC3480001u, 0x40800000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00000019u}, false},
        // max-u32-suppresses-height
        {{0x46800000u, 0x4347C000u, 0x46800000u, 0xC6800000u,
          0x00000000u, 0xC6800000u, 0xFFFFFFFFu}, true},
        // high-u32-equal
        {{0x46800000u, 0x46800000u, 0x00000000u, 0xC6800000u,
          0xC6800000u, 0x00000000u, 0x80000000u}, true},
        // high-u32-over-by-one
        {{0x46800000u, 0x46800000u, 0x3F800000u, 0xC6800000u,
          0xC6800000u, 0x00000000u, 0x80000000u}, false},
        // retained-height-order
        {{0x33200000u, 0x43480000u, 0x33200000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00009C40u}, true},
        // binary64-erases-distance-excess
        {{0xA6800000u, 0x00000000u, 0x00000000u, 0x43C80000u,
          0x00000000u, 0x00000000u, 0x00027100u}, false},
        // negative-zero-before-suppression
        {{0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u,
          0x00000000u, 0x00000000u, 0x00000000u}, true},
        // positive-zero-before-suppression
        {{0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u,
          0x80000000u, 0x80000000u, 0x00000000u}, true},
        // least-normal-extended-stays-positive
        {{0x00800000u, 0x00000000u, 0x00000000u, 0x00000000u,
          0x00000000u, 0x00000000u, 0x00000000u}, false},
    }};

    struct Captures
    {
        DoorCapture door;
        NodeCapture player;
    };

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    void WriteWord(std::uint8_t* bytes, std::uint32_t value) noexcept
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
    }

    Captures MakeCaptures(const Words& words) noexcept
    {
        Captures result{};
        for (std::size_t i = 0; i < result.door.size(); ++i)
            result.door[i] = static_cast<std::uint8_t>(13 + 37 * i);
        for (std::size_t i = 0; i < result.player.size(); ++i)
            result.player[i] = static_cast<std::uint8_t>(29 + 53 * i);
        WriteWord(result.door.data(), 0x006FBDE0);
        result.door[0x169] = 0;
        for (std::size_t i = 0; i < DoorPosition.size(); ++i)
        {
            WriteWord(result.door.data() + DoorPosition[i], words[i]);
            WriteWord(result.player.data() + NodePosition[i], words[i + 3]);
        }
        WriteWord(result.door.data() + 0x128, words[6]);
        return result;
    }

    bool RunCase(const Words& words, bool previous = false)
    {
        const auto capture = MakeCaptures(words);
        const auto before = capture;
        auto result = previous;
        Require(Door::TryEvaluatePcCachedDistanceForAnalysis(capture.door, capture.player, result),
            "qualified door and player captures were refused");
        Require(capture.door == before.door && capture.player == before.player, "distance modified a captured record");
        return result;
    }

    Words Parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        Words result{};
        for (auto& word : result)
        {
            std::string field;
            if (!(fields >> field) || field.size() != 8 || field.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("expected seven raw lowercase DWORDs");
            const auto end = field.data() + field.size();
            const auto parsed = std::from_chars(field.data(), end, word, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != end)
                throw std::runtime_error("invalid raw DWORD");
        }
        std::string extra;
        if (fields >> extra)
            throw std::runtime_error("unexpected trailing replay field");
        return result;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
            std::cout << (RunCase(Parse(line)) ? "01" : "00") << '\n';
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    void CheckRefusal(const Captures& capture, bool previous)
    {
        const auto before = capture;
        auto result = previous;
        Require(!Door::TryEvaluatePcCachedDistanceForAnalysis(capture.door, capture.player, result)
                && result == previous && capture.door == before.door && capture.player == before.player,
            "unqualified capture affected the output or a captured record");
    }

    int RunTests()
    {
        for (const auto& witness : Witnesses)
            Require(RunCase(witness.words, !witness.expected) == witness.expected, "PC cached distance decision differs");

        constexpr std::array<std::uint32_t, 6> invalidWords{
            0x00000001u, 0x807FFFFFu, 0x7F800000u, 0xFFC12345u, 0x46800001u, 0xC6800001u,
        };
        for (std::size_t lane = 0; lane < invalidWords.size(); ++lane)
        {
            auto words = Witnesses[0].words;
            words[lane] = invalidWords[lane];
            CheckRefusal(MakeCaptures(words), lane % 2 != 0);
        }
        auto wrongType = MakeCaptures(Witnesses[0].words);
        wrongType.door[0] ^= 1;
        CheckRefusal(wrongType, true);
        auto activeFlag = MakeCaptures(Witnesses[0].words);
        activeFlag.door[0x169] = 0x80;
        CheckRefusal(activeFlag, true);

        std::cout << "PivotingDoorCachedDistance: " << checks << " contracts passed\n";
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
