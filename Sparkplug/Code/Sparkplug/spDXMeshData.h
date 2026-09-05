#pragma once

// Inferred declaration path. The exact serializer translation unit survives
// as Code/Sparkplug/spDXMeshDataSerializer.cpp, but no source path for this
// data class itself is present in either executable.

#include "spMeshData.h"
#include "spPlatformSpecificMeshData.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spDXMeshData final : public spPlatformSpecificMeshData
    {
    public:
        static constexpr spClassID ClassID = 0x3178114C;

        spDXMeshData() noexcept = default;
        ~spDXMeshData() override;

        spDXMeshData(const spDXMeshData&) = delete;
        spDXMeshData& operator=(const spDXMeshData&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native PS2 0x0015CEB0 creates independent buffer copies from an
        // spMeshData. The routine is nonvirtual and distinct from RTTI clone.
        [[nodiscard]] bool InitializeFromMeshDataForAnalysis(
            const spMeshData& meshData);
        void ReleaseBuffersForAnalysis() noexcept;

        [[nodiscard]] const spIndexBuffer* GetIndexBufferForAnalysis() const noexcept;
        [[nodiscard]] const spVertexBuffer* GetVertexBufferForAnalysis() const noexcept;

    private:
        // Native offsets are +0x18/+0x1C, after an unresolved word at +0x14.
        // Portable owners do not claim either native 32-bit layout.
        std::unique_ptr<spIndexBuffer> indexBuffer_;
        std::unique_ptr<spVertexBuffer> vertexBuffer_;
    };
}
