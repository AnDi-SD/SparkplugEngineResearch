#include "wxMovingPlatform.h"
#include "Analysis/PC/wxMovingPlatformAbi.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/spSphereContactMath.h"
#include <cmath>
#include <cstring>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::analysis::pc::moving_platform;
        namespace Math = sparkplug::evidence::pc::sphere_contact_math;
        static_assert(sizeof(wxMovingPlatform::CapturedPrefixForAnalysis) == Abi::CapturedPrefixBytes);

        std::uint32_t ReadWord(const wxMovingPlatform::CapturedPrefixForAnalysis& bytes,
            const std::size_t offset) noexcept
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }

        bool Admitted(const float value) noexcept
        { return std::isfinite(value) && std::abs(value) <= 16384.0F; }
    }

    wxMovingPlatform::NearResultForAnalysis wxMovingPlatform::UpdateNearStateForAnalysis(
        const CapturedPrefixForAnalysis& before, const std::uint32_t capturedGlobalWord,
        const sparkplug::reconstruction::spNode* borrowedNode,
        const CapturedPointForAnalysis& point, CapturedPrefixForAnalysis& output,
        bool& returned, const NearProfileForAnalysis profile) noexcept
    {
        if (profile != NearProfileForAnalysis::PcPristineGlobalZeroFiniteCoordinates)
            return NearResultForAnalysis::UnqualifiedProfile;
        if (ReadWord(before, 0) != Abi::PrimaryTable)
            return NearResultForAnalysis::CapturedPrimaryNotAdmitted;
        if (capturedGlobalWord != 0)
            return NearResultForAnalysis::GlobalWordNotAdmitted;
        if (!borrowedNode)
            return NearResultForAnalysis::BorrowedNodeMissing;

        const auto& nodePosition = borrowedNode->GetWorldPositionForAnalysis();
        std::array<float, 3> pointPosition;
        std::memcpy(pointPosition.data(), point.data() + Abi::PointPositionOffset, 12);
        for (std::size_t lane = 0; lane != 3; ++lane)
            if (!Admitted(nodePosition[lane]) || !Admitted(pointPosition[lane]))
                return NearResultForAnalysis::GeometryNotAdmitted;

        // 57E801..57E824: X,Y,Z differences stay in nearest/64 precision.
        // The squared sum is (X*X + Z*Z) + Y*Y, with no binary32 spill.
        const auto x = Math::Subtract(Math::From(nodePosition[0]), Math::From(pointPosition[0]));
        const auto y = Math::Subtract(Math::From(nodePosition[1]), Math::From(pointPosition[1]));
        const auto z = Math::Subtract(Math::From(nodePosition[2]), Math::From(pointPosition[2]));
        const auto xz = Math::Add(Math::Multiply(x, x), Math::Multiply(z, z));
        const auto squaredDistance = Math::Add(xz, Math::Multiply(y, y));

        auto staged = before;
        staged[Abi::NearByteOffset] = 1;
        if (Math::nearest::Compare(squaredDistance, Math::From(2000000.0F)) > 0)
            staged[Abi::NearByteOffset] = 0;
        output = staged;
        returned = true;
        return NearResultForAnalysis::Completed;
    }
}
