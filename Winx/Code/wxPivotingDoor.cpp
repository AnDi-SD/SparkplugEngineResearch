#include "wxPivotingDoor.h"

#include "Analysis/PC/wxPivotingDoorAbi.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        namespace math = sparkplug::evidence::pc::sphere_contact_math;
        using Door = wxPivotingDoor::CapturedDoorPrefixForAnalysis;
        using Node = wxPivotingDoor::BorrowedPlayerNodePrefixForAnalysis;
        using Position = std::array<std::uint32_t, 3>;

        static_assert(Door{}.size() == Abi::wxPivotingDoorCaptureBytes);
        static_assert(Node{}.size() == Abi::wxPivotingDoorPlayerNodePrefixBytes);
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }

        Position ReadPosition(const std::uint8_t* bytes) noexcept
        {
            return {ReadWord(bytes), ReadWord(bytes + 4), ReadWord(bytes + 8)};
        }

        bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (0x00800000u <= magnitude && magnitude <= 0x46800000u);
        }

        math::Number FromBits(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }
    }

    bool wxPivotingDoor::TryEvaluatePcCachedDistanceForAnalysis(const CapturedDoorPrefixForAnalysis& door,
        const BorrowedPlayerNodePrefixForAnalysis& playerNode, bool& withinThreshold) noexcept
    {
        if (ReadWord(door.data()) != Abi::wxPivotingDoorPrimary || door[Abi::wxPivotingDoorInteractionFlag] != 0)
            return false;

        const auto point = ReadPosition(door.data() + Abi::wxPivotingDoorCachedX);
        const auto player = ReadPosition(playerNode.data() + Abi::wxPivotingDoorPlayerNodeX);
        if (!std::all_of(point.begin(), point.end(), Admitted) || !std::all_of(player.begin(), player.end(), Admitted))
            return false;

        const auto x = math::Subtract(FromBits(point[0]), FromBits(player[0]));
        auto y = math::Subtract(FromBits(point[1]), FromBits(player[1]));
        const auto z = math::Subtract(FromBits(point[2]), FromBits(player[2]));
        auto height = y;
        height.negative = false;
        if (math::nearest::Compare(height, math::From(Abi::wxPivotingDoorHeightTolerance)) < 0)
            y = {};

        const auto zy = math::Add(math::Multiply(z, z), math::Multiply(y, y));
        const auto squared = math::Add(zy, math::Multiply(x, x));
        // The original FILD plus high-bit correction is exact for every DWORD.
        const auto threshold = ReadWord(door.data() + Abi::wxPivotingDoorSquaredThreshold);
        const auto exactLimit = math::nearest::NormalizeNearest(math::Wide::Shifted(threshold, 0), 0, false);
        withinThreshold = math::nearest::Compare(squared, exactLimit) <= 0;
        return true;
    }
}
