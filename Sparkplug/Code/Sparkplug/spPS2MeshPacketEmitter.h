#pragma once

// Shared, bounded bytes of original15EA70/15EDB0 and their supported component
// producers. Internal transport, not a native object layout or DMA backend.
#include "spPS2MeshData.h"

namespace sparkplug::reconstruction::ps2packet
{
    struct Chunk final
    {
        std::vector<std::uint32_t> indices;
        // One original ADC byte per index. Any nonzero byte becomes8000 in
        // the position record's fourth word. No other component consumes ADC.
        std::vector<std::uint8_t> adc;
    };

    struct Emission final
    {
        spPS2MeshData::PacketBuildStatusForAnalysis status =
            spPS2MeshData::PacketBuildStatusForAnalysis::InvalidInput;
        std::vector<std::byte> bytes;
    };

    // Zero-initialized owning transport. The outer Type2 builder requests its
    // RET tag; direct-strip payloads omit it. Native padding not written by an
    // emitter is therefore represented only for an explicitly zeroed input.
    [[nodiscard]] Emission Emit(const spVertexBuffer& source,
        const spPS2MeshData::PreparationPlan& plan,
        const std::array<std::uint32_t, spPS2MeshData::FieldTableCount>& descriptors,
        const std::array<std::int32_t, spPS2MeshData::FieldTableCount>& componentOrder,
        std::uint32_t grouping, const std::vector<Chunk>& chunks,
        bool includeRet, std::uint32_t maximumBytes);
}
