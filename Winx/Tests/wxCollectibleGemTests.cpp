#include "Code/wxCollectibleGem.h"
#include "Code/wxEntity.h"
#include "Code/Sparkplug/spNode.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    unsigned contracts = 0;

    void Require(bool condition, const char* message)
    {
        ++contracts;
        if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }

    float Float(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    unsigned Nibble(char value)
    {
        if (value >= '0' && value <= '9') return unsigned(value - '0');
        if (value >= 'A' && value <= 'F') return unsigned(value - 'A') + 10;
        if (value >= 'a' && value <= 'f') return unsigned(value - 'a') + 10;
        throw std::logic_error("Invalid hexadecimal input byte");
    }

    template<std::size_t Size>
    void Decode(const std::string& text, std::array<std::uint8_t, Size>& bytes)
    {
        Require(text.size() == Size * 2, "exact projected byte extent");
        for (std::size_t i = 0; i != Size; ++i)
            bytes[i] = std::uint8_t((Nibble(text[i * 2]) << 4) | Nibble(text[i * 2 + 1]));
    }

    template<std::size_t Size>
    std::string Hex(const std::array<std::uint8_t, Size>& bytes)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string text;
        text.reserve(Size * 2);
        for (auto value : bytes)
        {
            text.push_back(digits[value >> 4]);
            text.push_back(digits[value & 15]);
        }
        return text;
    }

    struct Input
    {
        wxEntity::OwnBytesForAnalysis baseBefore;
        wxCollectibleGem::OwnBytesForAnalysis gemBefore;
        std::array<std::uint32_t, 6> positionWords;
        unsigned timerByte;
        std::uint32_t initialFlags;
        unsigned expectedReturn;
        std::uint32_t expectedFlags;
    };

    bool ReadInput(std::istream& input, Input& value)
    {
        std::string line;
        if (!std::getline(input, line)) return false;
        std::istringstream row(line);
        std::string baseHex, gemHex;
        row >> baseHex >> gemHex;
        Decode(baseHex, value.baseBefore);
        Decode(gemHex, value.gemBefore);
        for (auto& word : value.positionWords) row >> std::hex >> word;
        row >> std::dec >> value.timerByte >> std::hex >> value.initialFlags;
        row >> std::dec >> value.expectedReturn >> std::hex >> value.expectedFlags;
        Require(bool(row), "complete projected replay row");
        row >> std::ws;
        Require(row.eof(), "projected replay row reaches EOF");
        Require(value.timerByte <= 255 && value.expectedReturn <= 1, "projected scalar input range");
        return true;
    }

    struct Host final : wxEntityHost
    {
        bool timer = false;
        std::uint32_t boundFlags = 0;
        unsigned writes = 0;
        mutable unsigned timers = 0;
        mutable unsigned predicateCounts = 0;

        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void MoveFromEngineToGameForAnalysis(wxEntity&) override {}
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override {}
        void DestroyEntityForAnalysis(spEntity&) noexcept override {}
        std::uint16_t GetReferenceCountForAnalysis(void*) override
        { throw std::logic_error("Gem replay has no bound reference lifetime"); }
        void SetReferenceCountForAnalysis(void*, std::uint16_t) override
        { throw std::logic_error("Gem replay has no bound reference lifetime"); }
        void DeleteReferenceForAnalysis(void*) override
        { throw std::logic_error("Gem replay has no bound reference lifetime"); }
        void CompleteCopyForAnalysis(const wxEntity&, wxEntity&,
            spCloneManager&, std::uint32_t) const override
        { throw std::logic_error("Gem replay has no Copy profile"); }
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override
        { ++predicateCounts; return 0; }
        bool CachedPredicateForAnalysis(const wxEntity&, std::size_t) const override
        { throw std::logic_error("Gem replay requires an empty predicate cache"); }
        bool TimerByteForAnalysis(const wxEntity&) const override
        { ++timers; return timer; }
        float SquaredDistanceForAnalysis(const wxEntity&) const override
        { throw std::logic_error("Gem replay requires the Entity threshold sentinel"); }
        void WriteComputedFlagsForAnalysis(wxEntity&, std::uint32_t flags) override
        { boundFlags = flags; ++writes; }
    };

    auto NodeSnapshot(const spNode& node)
    {
        return std::make_tuple(node.GetPositionForAnalysis(), node.GetScaleForAnalysis(),
            node.GetOrientationForAnalysis(), node.GetWorldPositionForAnalysis(),
            node.GetWorldScaleForAnalysis(), node.GetWorldOrientationForAnalysis(),
            node.GetFlagsForAnalysis(), node.GetParentForAnalysis(),
            node.GetChildCountForAnalysis(), node.GetCollisionCountForAnalysis());
    }

    std::string Evaluate(const Input& input)
    {
        Host host;
        host.timer = input.timerByte != 0;
        host.boundFlags = input.initialFlags;
        wxEntity entity(host);
        entity.GetOwnBytesForAnalysis() = input.baseBefore;
        auto own = input.gemBefore;
        spNode first, second;
        first.SetPositionForAnalysis({Float(input.positionWords[0]), Float(input.positionWords[1]), Float(input.positionWords[2])});
        second.SetPositionForAnalysis({Float(input.positionWords[3]), Float(input.positionWords[4]), Float(input.positionWords[5])});
        first.MarkLocalTransformDirtyForAnalysis();
        second.MarkLocalTransformDirtyForAnalysis();
        Require(first.UpdateWorldForAnalysis() && second.UpdateWorldForAnalysis(), "Node world cache setup");
        const auto firstBefore = NodeSnapshot(first);
        const auto secondBefore = NodeSnapshot(second);
        std::array<std::uint8_t, 24> positionsBefore;
        std::memcpy(positionsBefore.data(), first.GetWorldPositionForAnalysis().data(), 12);
        std::memcpy(positionsBefore.data() + 12, second.GetWorldPositionForAnalysis().data(), 12);
        Require(std::memcmp(positionsBefore.data(), input.positionWords.data(), 24) == 0,
            "supplied float words reach the actual Node world caches unchanged");

        const bool result = wxCollectibleGem::PredicateForAnalysis(entity, own,
            host.boundFlags, &first, &second);
        const bool disabled = own[0x149 - 0x124] != 0;
        const bool enabled = input.baseBefore[0x38 - 0x28] != 0;
        Require(result == bool(input.expectedReturn), "Gem return matches the native projection");
        Require(host.boundFlags == input.expectedFlags, "Gem bound flag word matches the native projection");
        Require(host.writes == unsigned(!disabled), "base flag write count and early disable");
        Require(host.timers == unsigned(!disabled), "timer access count and early disable");
        Require(host.predicateCounts == unsigned(!disabled && enabled), "empty cache access and early disable");
        Require(entity.GetOwnBytesForAnalysis() == input.baseBefore && own == input.gemBefore,
            "all supplied common and Gem own bytes are preserved");
        Require(NodeSnapshot(first) == firstBefore && NodeSnapshot(second) == secondBefore,
            "represented Node transforms, flags, parent and child/collision counts are preserved");
        std::array<std::uint8_t, 24> positionsAfter;
        std::memcpy(positionsAfter.data(), first.GetWorldPositionForAnalysis().data(), 12);
        std::memcpy(positionsAfter.data() + 12, second.GetWorldPositionForAnalysis().data(), 12);
        std::ostringstream output;
        output << unsigned(result) << ' ' << std::hex << std::uppercase << std::setfill('0')
            << std::setw(8) << host.boundFlags << ' ' << Hex(entity.GetOwnBytesForAnalysis())
            << ' ' << Hex(own) << ' ' << Hex(positionsAfter);
        return output.str();
    }

    Input UnitInput(unsigned disabled, unsigned overrideByte, unsigned enabled,
        unsigned pause, const std::array<float, 3>& second, float radius, bool expected)
    {
        Input value;
        value.baseBefore.fill(0xA5);
        value.baseBefore[0] = 1;
        value.baseBefore[0x38 - 0x28] = std::uint8_t(enabled);
        value.baseBefore[0x39 - 0x28] = 1;
        const std::uint32_t sentinel = 0x7F7FFFFF;
        std::memcpy(value.baseBefore.data() + 0x3C - 0x28, &sentinel, 4);
        value.gemBefore.fill(0xA5);
        std::memcpy(value.gemBefore.data() + 0x140 - 0x124, &radius, 4);
        value.gemBefore[0x148 - 0x124] = std::uint8_t(overrideByte);
        value.gemBefore[0x149 - 0x124] = std::uint8_t(disabled);
        value.positionWords.fill(0);
        std::memcpy(value.positionWords.data() + 3, second.data(), 12);
        value.timerByte = pause;
        value.initialFlags = 0xA5A5A5A5;
        value.expectedReturn = unsigned(expected);
        value.expectedFlags = disabled ? value.initialFlags :
            0xFFFFFF00 | (enabled ? 2U : 0U) | (pause ? 8U : 0U);
        return value;
    }

    void Units()
    {
        Evaluate(UnitInput(1, 0, 1, 0, {3, 149, 4}, 5, false));
        Evaluate(UnitInput(255, 0, 1, 0, {3, 149, 4}, 5, false));
        Evaluate(UnitInput(0, 0, 0, 0, {3, 0, 4}, 5, true));
        Evaluate(UnitInput(0, 0, 0, 255, {3, 0, 4}, 5, false));
        Evaluate(UnitInput(0, 0, 1, 0, {3, 149, 4}, 5, true));
        Evaluate(UnitInput(0, 0, 1, 255, {3, 149, 4}, 5, true));
        Evaluate(UnitInput(0, 1, 0, 255, {3, 0, 4}, 5, true));
        Evaluate(UnitInput(0, 255, 1, 255, {3, 0, 4}, 5, true));
        Evaluate(UnitInput(0, 0, 1, 0, {3, 150, 4}, 5, false));
        Evaluate(UnitInput(0, 0, 1, 0, {3, -150, 4}, 151, true));
        Evaluate(UnitInput(0, 0, 1, 0, {3, 149, 4}, 4, false));
        Evaluate(UnitInput(0, 0, 1, 0, {3, 149, 4}, -5, true));
    }

    int Replay(std::istream& input)
    {
        Input value;
        unsigned rows = 0;
        while (ReadInput(input, value))
        {
            std::cout << Evaluate(value) << '\n';
            ++rows;
        }
        Require(input.eof() && rows != 0, "projected cases reach EOF");
        return EXIT_SUCCESS;
    }
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        Units();
        std::cout << "CollectibleGem: " << contracts << " contracts passed\n";
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string(argv[1]) == "--replay") return Replay(std::cin);
    if (argc == 2)
    {
        std::ifstream input(argv[1]);
        Require(bool(input), "projected case input opens");
        return Replay(input);
    }
    std::cerr << "Expected --replay or a projected input file\n";
    return EXIT_FAILURE;
}
