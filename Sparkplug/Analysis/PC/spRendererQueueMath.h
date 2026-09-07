#pragma once

// Analytical CPU helpers for PC454C30/454800/454970, not original symbols.
// Queue ownership, CRT tie order and renderer/Scene integration are NOT hidden
// inside this finite-input math slice. Native alpha comparator never returns0.
#include "spNodeTransformMath.h"
#include <cstdint>

namespace sparkplug::evidence::pc::renderer_queue_math
{
    using node_math::Matrix4;
    using node_math::Vector3;

    struct AlphaKey
    {
        float distanceSquared = 0;
        std::uint32_t priority = 0;
        bool exactParticleSystem = false;
    };

    inline Vector3 TransformPoint(const Vector3& point, const Matrix4& matrix) noexcept
    {
        Vector3 result{};
        for (std::size_t c = 0; c < 3; ++c)
            result[c] =
                static_cast<float>(static_cast<double>(point[2]) * matrix[8 + c] +
                                   static_cast<double>(point[0]) * matrix[c] +
                                   static_cast<double>(point[1]) * matrix[4 + c] + matrix[12 + c]);
        return result;
    }

    inline AlphaKey BuildAlphaKey(const Vector3& localCenter, const Matrix4& cachedWorld,
                                  const Matrix4& cachedView, const bool cameraBranch231,
                                  const std::uint32_t priority,
                                  const std::uint32_t rendererPriority48,
                                  const bool exactParticleSystem) noexcept
    {
        const auto world = TransformPoint(localCenter, cachedWorld);
        const auto view = TransformPoint(world, cachedView);
        const double z2 = static_cast<double>(view[2]) * view[2];
        const double distance = cameraBranch231 ? z2
                                                : static_cast<double>(view[0]) * view[0] +
                                                      static_cast<double>(view[1]) * view[1] + z2;
        return {static_cast<float>(distance), priority + rendererPriority48, exactParticleSystem};
    }

    inline int CompareAlpha(const AlphaKey& first, const AlphaKey& second) noexcept
    {
        if (first.exactParticleSystem != second.exactParticleSystem)
            return first.exactParticleSystem ? 1 : -1;
        if (!first.exactParticleSystem && first.priority != second.priority)
            return first.priority > second.priority ? -1 : 1;
        return first.distanceSquared > second.distanceSquared ? -1 : 1;
    }

    inline bool LessGeneral(const std::uint32_t firstMaterial, const std::uint32_t firstMesh,
                            const std::uint32_t secondMaterial,
                            const std::uint32_t secondMesh) noexcept
    {
        return firstMaterial < secondMaterial ||
               (firstMaterial == secondMaterial && firstMesh < secondMesh);
    }
} // namespace sparkplug::evidence::pc::renderer_queue_math
