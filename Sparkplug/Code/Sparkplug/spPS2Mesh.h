#pragma once

// Inferred declaration path. The PS2 executable preserves the class name and
// complete implementation, but no original header/source filename.

#include "spPS2MeshData.h"
#include "spRenderMesh.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    // Native PS2 render-mesh leaf. The hardware packet emitter at native +0x54
    // has no recovered public type, so the portable slice models only the
    // proven ownership and cached mesh metadata around it.
    class spPS2Mesh final : public spRenderMesh
    {
    public:
        static constexpr spClassID ClassID = 0x35ED77A5;

        spPS2Mesh() noexcept = default;
        ~spPS2Mesh() override;

        spPS2Mesh(const spPS2Mesh&) = delete;
        spPS2Mesh& operator=(const spPS2Mesh&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable ownership seam for PS2 0x001EF3C0. Native code takes an
        // already prepared spPS2MeshData, binds it to a packet emitter, then
        // copies data +0x34/+0x30 into common primitive/vertex count fields.
        [[nodiscard]] bool AttachPreparedDataForAnalysis(
            std::unique_ptr<spPS2MeshData> data,
            std::uint32_t primitiveCount,
            std::uint32_t vertexCount) noexcept;
        void ReleaseForAnalysis() noexcept;

        [[nodiscard]] const spPS2MeshData*
            GetPreparedDataForAnalysis() const noexcept;

    private:
        std::unique_ptr<spPS2MeshData> preparedData_;
    };
}
