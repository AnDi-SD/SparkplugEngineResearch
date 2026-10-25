#pragma once

// Inferred declaration path. The related serializer path survives exactly as
// Code/Sparkplug/spPS2MeshDataSerializer.cpp; the data-class TU does not.

#include "spIndexBuffer.h"
#include "spPlatformSpecificMeshData.h"
#include "spVertexBuffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPS2MeshData final : public spPlatformSpecificMeshData
    {
    public:
        static constexpr spClassID ClassID = 0x737D740F;
        static constexpr std::size_t FieldTableCount = 22;

        // Native 0x00160D10/0x00474FF0 accept an index buffer and a vertex
        // buffer, normalize the vertex layout, and then build PS2 DMA/VIF/GIF
        // data. This structure exposes only the hardware-independent planning
        // portion proven by both implementations.
        struct PreparationPlan final
        {
            bool accepted = false;
            bool alreadyPrepared = false;
            bool requiresExpandedVertexBuffer = false;
            std::uint32_t sourceComponentFlags = 0;
            std::uint32_t effectiveComponentFlags = 0;
            std::uint32_t field14 = 4;
            std::uint32_t field18 = 0;
            std::uint32_t field20 = 0;
            std::uint32_t field38 = 0;
            std::uint32_t field3C = 0;
            std::uint32_t sourceIndexCount = 0;
        };

        spPS2MeshData() noexcept;
        ~spPS2MeshData() override;

        spPS2MeshData(const spPS2MeshData&) = delete;
        spPS2MeshData& operator=(const spPS2MeshData&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] static PreparationPlan PlanPreparationForAnalysis(
            const spIndexBuffer& indices,
            const spVertexBuffer& vertices) noexcept;

        struct VertexNormalizationForAnalysis final
        {
            bool completed = false;
            bool borrowed = false;
            bool capturedAllocationUsed = false;
            const spVertexBuffer* buffer = nullptr;
            std::unique_ptr<spVertexBuffer> expanded;
            // One per destination byte:1 means native160900 writes this byte.
            // Other bytes retain captured allocation input, or explicit host0.
            std::vector<std::uint8_t> writtenBytes;
        };
        // Full raw-copy/default writes of160900. A compatible source is borrowed.
        // Captured allocation bytes let callers retain native untouched bytes;
        // without them, spVertexBuffer's own zero allocation is host transport.
        [[nodiscard]] static VertexNormalizationForAnalysis NormalizeVertexBufferForAnalysis(
            const spVertexBuffer& source,
            const std::vector<std::byte>* capturedAllocation = nullptr,
            std::uint32_t maximumBytes = 64U * 1024U * 1024U);

        enum class PacketBuildStatusForAnalysis
        {
            Built, AlreadyPreparedInput, RejectedNativeType, NativePreparationRejected, EmptyIndexStream,
            UnsupportedIndexPath, UnsupportedWeights, UnsupportedWeightedNormalization, UnsupportedComponents,
            UnsupportedCompressedNormal, InvalidInput, NormalizationIncomplete,
            ByteBudgetExceeded, AllocationFailed, InternalSizeMismatch, StripPreparationIncomplete,
        };
        struct PacketBuildResultForAnalysis final
        {
            bool completed = false;
            bool nativeReturn = false;
            PacketBuildStatusForAnalysis status = PacketBuildStatusForAnalysis::InvalidInput;
        };
        struct PacketInfoForAnalysis final
        {
            PreparationPlan plan;
            std::uint32_t grouping = 1;
            std::uint32_t qwords = 0;
            std::uint32_t emittedVertices = 0;
            std::uint32_t primitives = 0;
        };
        // Whole final packet for supported Type2 branches. Weighted input must
        // have contiguous1..4 weights and bone-index flag20. An expanded
        // weighted layout additionally requires captured allocation bytes for
        // native untouched weights/bones; absence remains explicit incomplete.
        // grouping==1
        // selects component streams; every other value selects interleaved.
        // Default compressed-normal grouped packing is explicitly incomplete.
        // Canonical PS2 Type3 preparation returns native false: its original
        // stripifier1C4FE0 unconditionally rejects the operation. No generated
        // strip object/packet or successful producer callback is invented.
        // Host guards/staging preserve prior state on failure; native early
        // release/fault/allocation side effects are not reproduced by this API.
        // This produces a safely sized owning packet, not native FC1 external
        // storage/cursor state. Native weighted interleaved FC1 can publish one
        // fewer QW than it writes; that observed metadata is NOT copied into
        // this packet's QWC or used to underallocate its storage.
        [[nodiscard]] PacketBuildResultForAnalysis BuildPacketForAnalysis(
            const spIndexBuffer& indices, const spVertexBuffer& vertices,
            std::uint32_t grouping = 1,
            std::uint32_t maximumBytes = 64U * 1024U * 1024U,
            const std::vector<std::byte>* capturedExpandedAllocation = nullptr);
        [[nodiscard]] const std::vector<std::byte>& PacketBytesForAnalysis() const noexcept;
        [[nodiscard]] const PacketInfoForAnalysis& PacketInfoForAnalysisView() const noexcept;

        struct StripPayloadForAnalysis final
        {
            bool completed = false;
            PacketBuildStatusForAnalysis status = PacketBuildStatusForAnalysis::InvalidInput;
            std::uint32_t emittedVertices = 0;
            std::vector<std::byte> bytes;
        };
        // Isolated direct15F1F0 route from prepopulated strip records. The
        // caller supplies compatible normalized VB and captured vertex plan;
        // this is separate from Type3's known native stripifier rejection.
        // Reuses shared component emitters with per-index ADC. Bytes represent
        // an explicitly zeroed destination and omit the OUTER RET tag. Return
        // extent models the direct end pointer, not outer meshQWC/ownership.
        // No instance packet/metadata or caller storage is changed on failure.
        [[nodiscard]] StripPayloadForAnalysis BuildPrepopulatedStripPayloadForAnalysis(
            const spVertexBuffer& vertices, const PreparationPlan& capturedPlan,
            const std::vector<std::vector<std::uint16_t>>& strips,
            std::uint32_t capacity, std::uint32_t grouping = 1,
            std::uint32_t maximumBytes = 64U * 1024U * 1024U) const;

        // Exact PS2-constructor defaults. Original member names have not been
        // recovered, so the offset-based names are intentionally retained.
        [[nodiscard]] std::uint32_t GetField14ForAnalysis() const noexcept;
        [[nodiscard]] const std::array<std::uint32_t, FieldTableCount>&
            GetField48ForAnalysis() const noexcept;
        [[nodiscard]] const std::array<std::int32_t, FieldTableCount>&
            GetFieldA0ForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFieldF8ForAnalysis() const noexcept;
        [[nodiscard]] bool HasPreparedPacketForAnalysis() const noexcept;
        [[nodiscard]] bool HasExternalPacketForAnalysis() const noexcept;

    private:
        std::uint32_t field14_ = 4;
        std::array<std::uint32_t, FieldTableCount> field48_{};
        std::array<std::int32_t, FieldTableCount> fieldA0_{};
        std::uint32_t fieldF8_ = 1;

        // Final packet bytes are evidence, not executable host/PS2 DMA memory.
        bool hasPreparedPacket_ = false;
        bool externalPacket_ = false;
        std::vector<std::byte> packetBytes_;
        PacketInfoForAnalysis packetInfo_;
    };
}
