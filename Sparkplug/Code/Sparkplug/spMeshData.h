#pragma once

// Inferred declaration path. Native RTTI and complete layout are confirmed,
// but no original header or translation-unit path survives in either build.

#include "spMesh.h"
#include "spVertexBuffer.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spMeshData final : public spMesh
    {
    public:
        static constexpr spClassID ClassID = 0x33C34CF0;

        spMeshData() noexcept = default;
        ~spMeshData() override;

        spMeshData(const spMeshData&) = delete;
        spMeshData& operator=(const spMeshData&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native initialization deep-copies both inputs before running the
        // inherited bounds pass. This portable facade keeps that ownership.
        [[nodiscard]] bool InitializeForAnalysis(
            const spIndexBuffer& indices,
            const spVertexBuffer& vertices);
        [[nodiscard]] std::unique_ptr<spMeshData>
            CopyMeshDataForAnalysis() const;
        void ReleaseBuffersForAnalysis() noexcept;

        [[nodiscard]] const spIndexBuffer* GetIndexBufferForAnalysis() const noexcept;
        [[nodiscard]] const spVertexBuffer* GetVertexBufferForAnalysis() const noexcept;

    private:
        std::unique_ptr<spIndexBuffer> indexBuffer_;
        std::unique_ptr<spVertexBuffer> vertexBuffer_;
    };
}
