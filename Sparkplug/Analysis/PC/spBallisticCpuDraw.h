#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include "spNodeTransformMath.h"

namespace sparkplug::evidence::pc
{
    // Semantic reconstruction of PC4B9830/4BA910 CPU quad expressions,
    // separate from BallisticPFX.vsh, particle simulation and device submission.
    using BallisticCpuParticleRecord = std::array<float, 8>; // P, V, birth, lifetime
    struct BallisticCpuParametersForAnalysis
    {
        float currentTime = 0; // ParticleSystem+A4
        std::array<float, 3> accelerationBegin{}, accelerationEnd{}; // +CC/+D8
        std::array<float, 2> runtimeScales{1,1}; // +100/+104, not raw serialized fields
    };
    struct BallisticCpuSampleForAnalysis
    {
        double age = 0, normalizedAge = 0;
        std::array<float, 3> center{};
        float halfSize = 0;
    };

    // PC4BA910: the current record position is already the draw center.
    // Velocity and acceleration do not participate; age is not wrapped.
    [[nodiscard]] inline bool EvaluateBallisticCpuStoredPositionForAnalysis(
        const BallisticCpuParticleRecord& record, float currentTime,
        const std::array<float, 2>& runtimeScales,
        BallisticCpuSampleForAnalysis& output) noexcept
    {
        for (const float value : record) if (!std::isfinite(value)) return false;
        for (const float value : runtimeScales) if (!std::isfinite(value)) return false;
        if (!std::isfinite(currentTime) || !(record[7] > 0)) return false;
        BallisticCpuSampleForAnalysis result;
        result.age = double(currentTime) - record[6];
        result.normalizedAge = result.age / record[7];
        result.center = {record[0], record[1], record[2]};
        result.halfSize = float(((1.0 - result.normalizedAge) * runtimeScales[0]
            + result.normalizedAge * runtimeScales[1]) * 0.5);
        if (!std::isfinite(result.halfSize)) return false;
        output = result;
        return true;
    }

    struct BallisticCpuBasisForAnalysis
    {
        std::array<float, 3> right{}, up{}, forward{};
    };

    // PC4BB100 uses global identity orientation/unit scale for local particles;
    // only node translation enters SetWorld. The per-quad basis uses node scale
    // separately. This is not the general RenderNode affine transform.
    [[nodiscard]] inline bool BuildBallisticCpuWorldMatrixForAnalysis(
        const std::array<float, 3>& nodePosition, bool worldSpace,
        std::array<float, 16>& output) noexcept
    {
        for (const float value : nodePosition) if (!std::isfinite(value)) return false;
        output = node_math::Affine(worldSpace ? std::array<float,3>{} : nodePosition,
            node_math::Identity, {1,1,1});
        return true;
    }
    // Both CPU draw branches build the same basis. The node's position and
    // orientation do not affect these xyz expressions; its scale does.
    // Reuses the common PC vector math; this is not bit-exact x87.
    // Forward is normalized BEFORE component-wise scale and stays unnormalized
    // afterwards, including during the near-parallel test against global up.
    [[nodiscard]] inline bool BuildBallisticCpuBasisForAnalysis(
        const std::array<float, 9>& cameraOrientation,
        const std::array<float, 3>& nodeScale,
        BallisticCpuBasisForAnalysis& output) noexcept
    {
        using node_math::Cross;
        using node_math::Normalize;
        for (const float value : cameraOrientation) if (!std::isfinite(value)) return false;
        for (const float value : nodeScale) if (!std::isfinite(value)) return false;
        BallisticCpuBasisForAnalysis result;
        result.forward = {-cameraOrientation[6], -cameraOrientation[7], -cameraOrientation[8]};
        if (!std::isfinite(Normalize(result.forward))) return false;
        for (unsigned c=0; c<3; ++c) result.forward[c] *= nodeScale[c];
        result.up = {0,1,0};
        if (std::abs(std::abs(double(result.forward[1])) - 1.0) <= double(0.001f))
        {
            result.up = {cameraOrientation[3], cameraOrientation[4], cameraOrientation[5]};
            if (!std::isfinite(Normalize(result.up))) return false;
            for (unsigned c=0; c<3; ++c) result.up[c] *= nodeScale[c];
        }
        result.right = Cross(result.up, result.forward);
        if (!std::isfinite(Normalize(result.right))) return false;
        result.up = Cross(result.forward, result.right);
        if (!std::isfinite(Normalize(result.up))) return false;
        for (const auto& vector : {result.forward,result.right,result.up})
            for (const float value : vector) if (!std::isfinite(value)) return false;
        output = result;
        return true;
    }

    using BallisticCpuQuadPositionsForAnalysis = std::array<std::array<float, 3>, 4>;
    // Vertex order pairs with UV (0,1), (0,0), (1,0), (1,1).
    [[nodiscard]] inline bool BuildBallisticCpuQuadPositionsForAnalysis(
        const BallisticCpuSampleForAnalysis& sample,
        const BallisticCpuBasisForAnalysis& basis,
        BallisticCpuQuadPositionsForAnalysis& output) noexcept
    {
        if (!std::isfinite(sample.halfSize)) return false;
        BallisticCpuQuadPositionsForAnalysis result{};
        constexpr int signs[4][2] = {{1,-1},{1,1},{-1,1},{-1,-1}};
        for (unsigned v=0; v<4; ++v) for (unsigned c=0; c<3; ++c)
        {
            const double direction = double(signs[v][0])*basis.right[c]
                + double(signs[v][1])*basis.up[c];
            result[v][c] = float(double(sample.center[c]) + double(sample.halfSize)*direction);
            if (!std::isfinite(result[v][c])) return false;
        }
        output = result;
        return true;
    }

    // Positive lifetime, finite values, bounded conversion range and unchanged
    // output on refusal are host restrictions. This is not bit-exact x87.
    // The original scalar path does not test the particle loop flag here.
    // Packed color is intentionally outside this interface: x87 conversion
    // at integer boundaries differs from ordinary double arithmetic.
    [[nodiscard]] inline bool EvaluateBallisticCpuMotionForAnalysis(
        const BallisticCpuParticleRecord& record,
        const BallisticCpuParametersForAnalysis& parameters,
        BallisticCpuSampleForAnalysis& output) noexcept
    {
        for (const float value : record) if (!std::isfinite(value)) return false;
        for (const float value : parameters.accelerationBegin) if (!std::isfinite(value)) return false;
        for (const float value : parameters.accelerationEnd) if (!std::isfinite(value)) return false;
        for (const float value : parameters.runtimeScales) if (!std::isfinite(value)) return false;
        if (!std::isfinite(parameters.currentTime) || !(record[7] > 0)) return false;
        const double lifetime = record[7];
        BallisticCpuSampleForAnalysis result;
        result.age = double(parameters.currentTime) - record[6];
        // Actual conversion consumes the low unsigned word of _ftol2. Values
        // outside this non-wrapping domain have not been qualified here.
        if (std::abs(result.age) >= 4294967296.0 || std::abs(result.age / lifetime) >= 4294967296.0)
            return false;
        if (result.age > lifetime) result.age -= std::trunc(result.age / lifetime) * lifetime;
        if (result.age < 0)
            result.age += (std::trunc(-result.age) / lifetime + 1.0) * lifetime;
        result.normalizedAge = result.age / lifetime;
        const double complement = 1.0 - result.normalizedAge;
        result.halfSize = float((complement * parameters.runtimeScales[0]
            + result.normalizedAge * parameters.runtimeScales[1]) * 0.5);
        bool constantAcceleration = true;
        for (unsigned component = 0; component < 3; ++component)
            constantAcceleration = constantAcceleration && std::abs(double(parameters.accelerationBegin[component])
                - parameters.accelerationEnd[component]) <= double(0.001f);
        const double squaredAge = result.age * result.age;
        for (unsigned component = 0; component < 3; ++component)
        {
            double position = record[component] + result.age * record[component + 3]
                + 0.5 * squaredAge * parameters.accelerationBegin[component];
            if (!constantAcceleration)
                position += double(0x1.555556p-3f) * squaredAge * result.normalizedAge
                    * (double(parameters.accelerationEnd[component]) - parameters.accelerationBegin[component]);
            result.center[component] = float(position);
            if (!std::isfinite(result.center[component])) return false;
        }
        if (!std::isfinite(result.halfSize)) return false;
        output = result;
        return true;
    }

    struct BallisticCpuBatchForAnalysis
    {
        std::uint32_t firstParticle = 0, particleCount = 0;
    };
    // PC4B9830 and4BA910 share this batching rule. Capacity is in vertices
    // and uint16 indices, not bytes. The zero final batch on exact multiples
    // is original behavior; do not replace it with a full batch.
    [[nodiscard]] inline bool PlanBallisticCpuBatchesForAnalysis(
        std::uint32_t activeCount, std::uint32_t vertexCapacity,
        std::uint32_t indexCapacity, std::vector<BallisticCpuBatchForAnalysis>& output)
    {
        const auto capacity = std::min(vertexCapacity / 4, indexCapacity / 6);
        if (!capacity || activeCount > 65536) return false; // Host allocation/domain guard.
        const auto count = activeCount / capacity + (activeCount % capacity != 0 ? 1u : 0u);
        std::vector<BallisticCpuBatchForAnalysis> result;
        result.reserve(count);
        for (std::uint32_t batch = 0; batch < count; ++batch)
            result.push_back({batch * capacity, batch + 1 == count ? activeCount % capacity : capacity});
        output = std::move(result);
        return true;
    }
}
