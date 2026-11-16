#include "wxCollectibleGem.h"
#include "Code/wxEntity.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    namespace
    {
        namespace math = sparkplug::evidence::pc::sphere_contact_math;
        using Number = math::Number;

        void RequireGeometryValue(float value)
        {
            if (!std::isfinite(value) || std::abs(value) > 16384.0F)
                throw std::logic_error("Gem geometry requires finite values within the absolute limit of 16384");
        }

        Number SquaredDistance(
            const sparkplug::reconstruction::spNode& first,
            const sparkplug::reconstruction::spNode& second)
        {
            const auto& a = first.GetWorldPositionForAnalysis();
            const auto& b = second.GetWorldPositionForAnalysis();
            Number delta[3];
            for (unsigned lane = 0; lane != 3; ++lane)
            {
                RequireGeometryValue(a[lane]);
                RequireGeometryValue(b[lane]);
                delta[lane] = math::Subtract(math::From(b[lane]), math::From(a[lane]));
            }
            auto height = delta[1];
            height.negative = false;
            if (math::nearest::Compare(height, math::From(150.0F)) < 0)
                delta[1] = math::From(0.0F);

            // Original597600 keeps every intermediate in nearest/64 x87
            // precision and adds Z squared plus X squared, then Y squared.
            const auto x = math::Multiply(delta[0], delta[0]);
            const auto y = math::Multiply(delta[1], delta[1]);
            const auto z = math::Multiply(delta[2], delta[2]);
            return math::Add(math::Add(z, x), y);
        }
    }

    bool wxCollectibleGem::PredicateForAnalysis(
        wxEntity& entity,
        const OwnBytesForAnalysis& ownBytes,
        const std::uint32_t& boundRecordFlags,
        const sparkplug::reconstruction::spNode* otherNode,
        const sparkplug::reconstruction::spNode* ownerNode)
    {
        if (ownBytes[0x149 - 0x124] != 0)
            return false;

        const auto& common = entity.GetOwnBytesForAnalysis();
        std::uint32_t threshold;
        std::memcpy(&threshold, common.data() + 0x3C - 0x28, sizeof(threshold));
        if (common[0] == 0 || threshold != 0x7F7FFFFF)
            throw std::logic_error("Gem predicate requires the warm sentinel Entity profile");

        entity.UpdateFlagsForAnalysis();
        const auto flags = boundRecordFlags;
        if (ownBytes[0x148 - 0x124] == 0 && (flags & 0x02) != 0)
        {
            if (!otherNode || !ownerNode)
                throw std::logic_error("Gem geometry requires both borrowed Nodes");
            float radius;
            std::memcpy(&radius, ownBytes.data() + 0x140 - 0x124, sizeof(radius));
            RequireGeometryValue(radius);
            const auto distance = SquaredDistance(*otherNode, *ownerNode);
            const auto squaredRadius = math::Multiply(math::From(radius), math::From(radius));
            if (math::nearest::Compare(squaredRadius, distance) >= 0)
                return true;
        }
        return ownBytes[0x148 - 0x124] != 0 || (flags & 0x1A) == 0;
    }
}
