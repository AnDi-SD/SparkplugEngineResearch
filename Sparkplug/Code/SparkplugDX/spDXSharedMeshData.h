#pragma once

// Inferred header path. The implementation path survives exactly as
// Z:\Sparkplug\Code\SparkplugDX\spDXSharedMeshData.cpp.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXIndexBuffer;
    class spDXVertexBuffer;

    // PC-only owner of a reusable index/vertex buffer pair. The shipped
    // class has an observed 0x1c-byte extent: spBaseObject followed by the
    // two intrusive buffer pointers at +0x14 and +0x18.
    class spDXSharedMeshData final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x293A2681;

        spDXSharedMeshData() noexcept = default;
        ~spDXSharedMeshData() override;

        spDXSharedMeshData(const spDXSharedMeshData&) = delete;
        spDXSharedMeshData& operator=(const spDXSharedMeshData&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Safe host counterpart of PC 0x004C29C0. Native always creates an
        // INDEX16/default-pool index buffer and an FVF-zero vertex buffer,
        // copies the complete payloads through Lock, and then unlocks both.
        [[nodiscard]] bool InitializeForAnalysis(
            const std::vector<std::byte>& indexData,
            const std::vector<std::byte>& vertexData);
        void ReleaseBuffersForAnalysis() noexcept;

        [[nodiscard]] std::shared_ptr<spDXIndexBuffer>
            GetIndexBufferForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spDXVertexBuffer>
            GetVertexBufferForAnalysis() const noexcept;

    private:
        std::shared_ptr<spDXIndexBuffer> indexBuffer_;
        std::shared_ptr<spDXVertexBuffer> vertexBuffer_;
    };
}
