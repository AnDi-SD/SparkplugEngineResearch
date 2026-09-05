#pragma once

// Inferred declaration path. The exact class name is present in the PC RTTI
// graph, but no original header or uniquely attributable translation-unit path
// survives in the executable.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXMesh;

    // Exact five-word result of native spDXCombinedVB helper 0x004C07E0.
    // The original record type/name is still hidden by SecuROM, so the suffix
    // keeps this portable representation out of the claimed native API.
    struct spDXCombinedVBRangeForAnalysis final
    {
        std::uint32_t indexBegin = 0;
        std::uint32_t vertexBegin = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t vertexStride = 0;
    };

    // PC-only serialization-side aggregate used by spDXSceneGraphOptimizer.
    // Native payload pointers live at +0x2c/+0x30 and sizes at +0x34/+0x38;
    // earlier list/map-like containers remain evidence-only.
    class spDXCombinedVB final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x4B18E622;

        spDXCombinedVB() noexcept = default;
        ~spDXCombinedVB() override;

        spDXCombinedVB(const spDXCombinedVB&) = delete;
        spDXCombinedVB& operator=(const spDXCombinedVB&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Safe ownership facade for the four serializer-visible native fields.
        // Construction of these aggregates from meshes is owned by the still-
        // protected spDXSceneGraphOptimizer path and is not claimed here.
        [[nodiscard]] bool InitializePayloadForAnalysis(
            const std::vector<std::byte>& indexData,
            const std::vector<std::byte>& vertexData);
        void ReleasePayloadForAnalysis() noexcept;

        [[nodiscard]] const std::vector<std::byte>&
            GetIndexDataForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<std::byte>&
            GetVertexDataForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetIndexByteSizeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexByteSizeForAnalysis() const noexcept;

        // Safe facade for the native +0x1c tree/map. Mesh lifetime remains
        // external, matching the observed non-owning lookup key. A range is
        // accepted only when it fits the aggregate's current byte payload.
        [[nodiscard]] bool RegisterMeshRangeForAnalysis(
            const spDXMesh& mesh,
            const spDXCombinedVBRangeForAnalysis& range);
        [[nodiscard]] bool GetMeshRangeForAnalysis(
            const spDXMesh& mesh,
            spDXCombinedVBRangeForAnalysis& range) const noexcept;
        [[nodiscard]] std::size_t GetMeshRangeCountForAnalysis() const noexcept;
        void ClearMeshRangesForAnalysis() noexcept;

    private:
        std::vector<std::byte> indexData_;
        std::vector<std::byte> vertexData_;
        std::unordered_map<const spDXMesh*, spDXCombinedVBRangeForAnalysis>
            meshRanges_;
    };
}
