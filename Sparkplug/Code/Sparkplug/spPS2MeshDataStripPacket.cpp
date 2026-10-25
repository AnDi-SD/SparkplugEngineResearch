#include "spPS2MeshData.h"
#include "spPS2MeshPacketEmitter.h"
#include "../../Analysis/PS2/spPS2MeshStripPreparation.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    spPS2MeshData::StripPayloadForAnalysis spPS2MeshData::BuildPrepopulatedStripPayloadForAnalysis(
        const spVertexBuffer& vertices, const PreparationPlan& plan,
        const std::vector<std::vector<std::uint16_t>>& strips,
        std::uint32_t capacity, std::uint32_t grouping, std::uint32_t maximumBytes) const
    {
        using Status = PacketBuildStatusForAnalysis;
        const auto reject = [](Status status) { return StripPayloadForAnalysis{false, status, 0, {}}; };
        // Direct entry receives already configured mesh/VB state. It does not
        // itself normalize, run the Type3 stripifier or allocate an outer RET.
        if (!vertices.IsInitializedForAnalysis() || strips.empty() || !plan.accepted
            || plan.alreadyPrepared || plan.requiresExpandedVertexBuffer
            || plan.sourceComponentFlags != vertices.GetComponentFlagsForAnalysis()
            || plan.effectiveComponentFlags != plan.sourceComponentFlags)
            return reject(Status::InvalidInput);
        std::uint64_t sourceCount = 0;
        for (const auto& strip : strips)
        {
            sourceCount += strip.size();
            if (sourceCount > std::numeric_limits<std::uint32_t>::max()
                || sourceCount * 16U > maximumBytes) return reject(Status::ByteBudgetExceeded);
        }
        auto prepared = sparkplug::evidence::ps2::PreparePrepopulatedMeshStripsForAnalysis(
            strips, capacity, maximumBytes);
        if (!prepared.completed) return reject(Status::StripPreparationIncomplete);
        try
        {
            std::vector<ps2packet::Chunk> chunks;
            chunks.reserve(prepared.strips.size());
            std::uint32_t emittedVertices = 0;
            for (auto& strip : prepared.strips)
            {
                ps2packet::Chunk chunk;
                chunk.indices.assign(strip.indices.begin(), strip.indices.end());
                chunk.adc = std::move(strip.adc);
                emittedVertices += static_cast<std::uint32_t>(chunk.indices.size());
                chunks.push_back(std::move(chunk));
            }
            auto emitted = ps2packet::Emit(vertices, plan, field48_, fieldA0_, grouping, chunks, false, maximumBytes);
            if (emitted.status != Status::Built) return reject(emitted.status);
            return {true, Status::Built, emittedVertices, std::move(emitted.bytes)};
        }
        catch (...) { return reject(Status::AllocationFailed); }
    }
}
