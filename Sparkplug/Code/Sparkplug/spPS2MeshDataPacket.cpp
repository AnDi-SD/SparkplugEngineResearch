#include "spPS2MeshData.h"
#include "spPS2MeshPacketEmitter.h"

#include <utility>

namespace sparkplug::reconstruction
{
    spPS2MeshData::PacketBuildResultForAnalysis spPS2MeshData::BuildPacketForAnalysis(
        const spIndexBuffer& indices, const spVertexBuffer& vertices,
        std::uint32_t grouping, std::uint32_t maximumBytes,
        const std::vector<std::byte>* capturedExpandedAllocation)
    {
        using Status = PacketBuildStatusForAnalysis;
        const auto incomplete = [](Status status) { return PacketBuildResultForAnalysis{false, false, status}; };
        const auto type = indices.GetTypeForAnalysis();
        if (type == spIndexBuffer::eIndexBufferType::Type4) return {true, true, Status::AlreadyPreparedInput};
        if (type != spIndexBuffer::eIndexBufferType::Type2 && type != spIndexBuffer::eIndexBufferType::Type3)
            return {true, false, Status::RejectedNativeType};
        // 1C4FE0 returns0 for every type; caller160C00 immediately propagates0.
        // This is a known original rejection, not a completed packet build.
        if (type == spIndexBuffer::eIndexBufferType::Type3) return {true, false, Status::NativePreparationRejected};
        if (!indices.IsInitializedForAnalysis() || !vertices.IsInitializedForAnalysis()
            || indices.GetIndexElementSizeForAnalysis() != 2) return incomplete(Status::InvalidInput);
        const auto flags = vertices.GetComponentFlagsForAnalysis();
        const auto weightBits = (flags >> 1) & 15U;
        const bool weighted = weightBits != 0;
        if ((flags & 0x3EU) && (!weighted || !(flags & 0x20U)
            || (weightBits & (weightBits + 1U)))) return incomplete(Status::UnsupportedWeights);
        constexpr std::uint32_t supported = 0x7F800U | 0x17EU;
        if (flags & ~supported) return incomplete(Status::UnsupportedComponents);
        bool missingUv = false;
        for (unsigned uv = 0; uv < 8; ++uv)
        {
            const bool present = (flags & (0x800U << uv)) != 0;
            if (present && missingUv) return incomplete(Status::UnsupportedComponents);
            if (!present) missingUv = true;
        }
        if (grouping == 1 && (flags & 0x40U)) return incomplete(Status::UnsupportedCompressedNormal);
        const auto count = indices.GetIndexCountForAnalysis();
        if (count == 0) return {true, false, Status::EmptyIndexStream};
        const auto plan = PlanPreparationForAnalysis(indices, vertices);
        // Native160900 leaves retained weights/bone indices untouched when
        // expanding. Without captured allocation bytes no packet is promised.
        if (weighted && plan.requiresExpandedVertexBuffer && !capturedExpandedAllocation)
            return incomplete(Status::UnsupportedWeightedNormalization);
        // 15F92C..15F9B8: weighted memory reservation changes362 to234.
        const auto capacity = ((weighted ? 234U : 362U) / (plan.field20 + 6U)) / 3U * 3U;
        if (!capacity) return incomplete(Status::InvalidInput);
        const auto normalized = NormalizeVertexBufferForAnalysis(vertices, capturedExpandedAllocation, maximumBytes);
        if (!normalized.completed) return incomplete(Status::NormalizationIncomplete);
        const auto& source = *normalized.buffer;
        // Every supported vertex emits at least a16-byte position record.
        // Bound staging before allocating index/ADC chunks as well as output.
        if (std::uint64_t{count} * 16U > maximumBytes) return incomplete(Status::ByteBudgetExceeded);
        try
        {
            std::vector<ps2packet::Chunk> chunks;
            for (std::uint32_t first = 0; first < count;)
            {
                const auto n = count - first < capacity ? count - first : capacity;
                ps2packet::Chunk chunk;
                chunk.indices.reserve(n); chunk.adc.assign(n, 0);
                for (std::uint32_t at = 0; at < n; ++at)
                {
                    const auto index = indices.GetIndexForAnalysis(first + at);
                    if (!index || *index >= source.GetVertexCountForAnalysis()) return incomplete(Status::InvalidInput);
                    chunk.indices.push_back(*index);
                }
                chunks.push_back(std::move(chunk)); first += n;
            }
            auto emitted = ps2packet::Emit(source, plan, field48_, fieldA0_, grouping, chunks, true, maximumBytes);
            if (emitted.status != Status::Built) return incomplete(emitted.status);
            const auto qwords = static_cast<std::uint32_t>(emitted.bytes.size() / 16U);
            packetBytes_ = std::move(emitted.bytes);
            packetInfo_ = {plan, grouping, qwords, count, indices.GetPrimitiveCountForAnalysis()};
            field14_ = plan.field14; fieldF8_ = grouping;
            hasPreparedPacket_ = true; externalPacket_ = false;
            return {true, true, Status::Built};
        }
        catch (...) { return incomplete(Status::AllocationFailed); }
    }

    const std::vector<std::byte>& spPS2MeshData::PacketBytesForAnalysis() const noexcept { return packetBytes_; }
    const spPS2MeshData::PacketInfoForAnalysis& spPS2MeshData::PacketInfoForAnalysisView() const noexcept { return packetInfo_; }
}
