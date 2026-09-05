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

        // The portable class does not pretend that a host byte vector is an
        // executable PS2 DMA chain. Packet building remains evidence-only.
        bool hasPreparedPacket_ = false;
        bool externalPacket_ = false;
    };
}
