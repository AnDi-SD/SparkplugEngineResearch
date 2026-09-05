#pragma once

// Inferred declaration path.  RTTI proves the type/base on both platforms;
// no original spMesh header or translation-unit path survives.

#include "spIndexBuffer.h"
#include "spResource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMesh : public spResource
    {
    public:
        static constexpr spClassID ClassID = 0x3F077B6C;
        using Position = std::array<float, 3>;
        using BoundingSphere = std::array<float, 4>;

        spMesh() noexcept = default;
        ~spMesh() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native PC reuses the root null-clone slot; PS2 emits the equivalent
        // class-local stub at 0x00159F60.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Analytical facade for the bounds pass proved by PC 0x00424220-range
        // and PS2 0x00159AE0/0x00159C80. Vertex-buffer decoding itself belongs
        // to a later class, so this seam accepts already decoded positions.
        [[nodiscard]] bool ComputeBoundsForAnalysis(
            const spIndexBuffer& indices,
            const std::vector<Position>& positions,
            std::uint32_t vertexBegin = 0) noexcept;
        void InvalidateBoundsForAnalysis() noexcept;
        [[nodiscard]] bool HasBoundsForAnalysis() const noexcept;
        [[nodiscard]] const Position& GetMinimumForAnalysis() const noexcept;
        [[nodiscard]] const Position& GetMaximumForAnalysis() const noexcept;
        [[nodiscard]] const BoundingSphere&
            GetBoundingSphereForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t
            GetVertexComponentFlagsForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetPrimitiveCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexCountForAnalysis() const noexcept;

    protected:
        void SetBoundingSphereForAnalysis(
            const BoundingSphere& sphere) noexcept;
        void MarkBoundsValidForAnalysis() noexcept;
        void SetMeshMetadataForAnalysis(
            std::uint32_t componentFlags,
            std::uint32_t primitiveCount,
            std::uint32_t vertexCount) noexcept;

    private:
        friend class spMeshData;

        bool boundsValid_ = false;
        BoundingSphere boundingSphere_{};
        Position minimum_{};
        Position maximum_{};
        std::uint32_t vertexComponentFlags_ = 0;
        std::uint32_t primitiveCount_ = 0;
        std::uint32_t vertexCount_ = 0;
    };
}
