#include "Code/wxMovingPlatform.h"
#include "Analysis/PC/wxMovingPlatformAbi.h"
#include "Code/Sparkplug/spNode.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

using Platform = winx::reconstruction::wxMovingPlatform;
using Node = sparkplug::reconstruction::spNode;
using Prefix = Platform::CapturedPrefixForAnalysis;
using Point = Platform::CapturedPointForAnalysis;
using Profile = Platform::NearProfileForAnalysis;
using Result = Platform::NearResultForAnalysis;

namespace
{
    unsigned checks = 0;
    void Require(const bool value, const char* message)
    { ++checks; if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }

    float Float(const std::uint32_t bits)
    { float value; std::memcpy(&value, &bits, 4); return value; }

    int Digit(const char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    template<std::size_t Size> bool Decode(const std::string& text, std::array<std::uint8_t, Size>& bytes)
    {
        if (text.size() != Size * 2) return false;
        for (std::size_t index = 0; index != Size; ++index)
        {
            const auto high = Digit(text[index * 2]), low = Digit(text[index * 2 + 1]);
            if (high < 0 || low < 0) return false;
            bytes[index] = std::uint8_t(high * 16 + low);
        }
        return true;
    }

    template<std::size_t Size> std::string Hex(const std::array<std::uint8_t, Size>& bytes)
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string text;
        for (const auto byte : bytes) { text += digits[byte >> 4]; text += digits[byte & 15]; }
        return text;
    }

    auto Snapshot(const Node& node)
    {
        return std::make_tuple(node.GetPositionForAnalysis(), node.GetScaleForAnalysis(),
            node.GetOrientationForAnalysis(), node.GetWorldPositionForAnalysis(),
            node.GetWorldScaleForAnalysis(), node.GetWorldOrientationForAnalysis(),
            node.GetFlagsForAnalysis(), node.GetParentForAnalysis(),
            node.GetChildCountForAnalysis(), node.GetCollisionCountForAnalysis());
    }

    std::string Evaluate(const Prefix& before, const std::uint32_t global,
        const std::array<std::uint32_t, 3>& nodeWords, const Point& point)
    {
        Node node;
        node.SetPositionForAnalysis({Float(nodeWords[0]), Float(nodeWords[1]), Float(nodeWords[2])});
        node.MarkLocalTransformDirtyForAnalysis();
        Require(node.UpdateWorldForAnalysis(), "borrowed Node world cache setup");
        const auto snapshot = Snapshot(node);
        const auto inputBefore = before;
        const auto pointBefore = point;
        std::array<std::uint8_t, 12> positionBefore;
        std::memcpy(positionBefore.data(), node.GetWorldPositionForAnalysis().data(), 12);
        Require(std::memcmp(positionBefore.data(), nodeWords.data(), 12) == 0,
            "supplied Node float words preserved by real world cache");
        Prefix output; output.fill(0x5C);
        bool returned = false;
        Require(Platform::UpdateNearStateForAnalysis(before, global, &node, point, output,
            returned, Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::Completed,
            "qualified near-state operation completes");
        Require(returned, "original low-byte return is one");
        auto opaque = output; opaque[0x168] = before[0x168];
        Require(opaque == before && before == inputBefore && point == pointBefore,
            "all receiver opaque bytes, input and point are preserved");
        Require(Snapshot(node) == snapshot, "Node transforms, graph fields and flags preserved");
        std::array<std::uint8_t, 12> positionAfter;
        std::memcpy(positionAfter.data(), node.GetWorldPositionForAnalysis().data(), 12);
        Require(positionAfter == positionBefore, "Node cached raw position bytes preserved");
        auto alias = before;
        bool aliasReturn = false;
        Require(Platform::UpdateNearStateForAnalysis(alias, global, &node, point, alias,
            aliasReturn, Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::Completed
            && alias == output && aliasReturn, "same-buffer output matches staged result");
        return std::string("1 ") + Hex(output) + ' ' + Hex(positionAfter) + ' ' + Hex(point);
    }

    Prefix Receiver(const std::uint8_t old)
    {
        Prefix bytes; bytes.fill(0xA5);
        const std::uint32_t primary = 0x00701048;
        std::memcpy(bytes.data(), &primary, 4);
        bytes[0x168] = old;
        return bytes;
    }

    Point PositionPoint(const std::array<float, 3>& position)
    {
        Point point; point.fill(0xC5);
        std::memcpy(point.data() + 0x18, position.data(), 12);
        return point;
    }

    void Unit(const std::array<float, 3>& position, const bool expected, const std::uint8_t old)
    {
        const auto output = Evaluate(Receiver(old), 0, {0, 0, 0}, PositionPoint(position));
        Prefix after;
        std::istringstream stream(output); std::string returned, own;
        stream >> returned >> own;
        Require(returned == "1" && Decode(own, after) && after[0x168] == unsigned(expected),
            "known boundary or distance state");
    }

    void Units()
    {
        Unit({0, 0, 0}, true, 0);
        Unit({1000, 1000, 0}, true, 0xFF);
        Unit({1000, 0, 1000}, true, 0xA5);
        Unit({-1000, -1000, 0}, true, 0);
        Unit({1001, 1000, 0}, false, 1);
        Unit({999, 1000, 0}, true, 0x5C);
        Unit({16384, -16384, 16384}, false, 0);
        Unit({Float(0x00000001), Float(0x80000001), -0.0F}, true, 0xCC);
        const auto before = Receiver(0xA5);
        const auto point = PositionPoint({0, 0, 0});
        Prefix output; output.fill(0x5C); const auto refused = output;
        bool returned = false;
        Node node;
        Require(Platform::UpdateNearStateForAnalysis(before, 0, &node, point, output, returned)
            == Result::UnqualifiedProfile && output == refused && !returned, "default refusal preserves outputs");
        auto foreign = before; foreign[0] = 0;
        Require(Platform::UpdateNearStateForAnalysis(foreign, 0, &node, point, output, returned,
            Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::CapturedPrimaryNotAdmitted
            && output == refused && !returned, "foreign primary refusal preserves outputs");
        Require(Platform::UpdateNearStateForAnalysis(before, 1, &node, point, output, returned,
            Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::GlobalWordNotAdmitted
            && output == refused && !returned, "nonzero global is explicitly unqualified");
        Require(Platform::UpdateNearStateForAnalysis(before, 0, nullptr, point, output, returned,
            Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::BorrowedNodeMissing
            && output == refused && !returned, "missing borrowed Node refusal preserves outputs");
        const auto invalid = PositionPoint({Float(0x7FC00000), 0, 0});
        Require(Platform::UpdateNearStateForAnalysis(before, 0, &node, invalid, output, returned,
            Profile::PcPristineGlobalZeroFiniteCoordinates) == Result::GeometryNotAdmitted
            && output == refused && !returned, "nonfinite geometry refusal preserves outputs");
    }
}

int main(const int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream stream(line); std::string own, pointText, extra;
            Prefix before; Point point; std::uint32_t global = 0;
            std::array<std::uint32_t, 3> nodeWords;
            Require(bool(stream >> own >> std::hex >> global >> nodeWords[0] >> nodeWords[1]
                >> nodeWords[2] >> pointText) && !(stream >> extra)
                && Decode(own, before) && Decode(pointText, point), "exact source projection row");
            std::cout << Evaluate(before, global, nodeWords, point) << '\n';
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) return EXIT_FAILURE;
    Units();
    std::cout << "MovingPlatformNear: " << checks << " contracts passed\n";
    return EXIT_SUCCESS;
}
