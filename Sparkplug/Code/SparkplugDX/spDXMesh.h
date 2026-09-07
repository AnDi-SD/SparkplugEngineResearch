#pragma once

// Inferred header path for classes compiled by the exact native translation
// unit Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp.

#include "../Sparkplug/spRenderMesh.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXIndexBuffer;
    class spDXSharedMeshData;
    class spDXVertexBuffer;
    class spMeshData;
    class spVertexBuffer;
    class spDXRenderer;
    class spPCVertexDeclaration;

    // Native helper has no Sparkplug RTTI record. Its only virtual entry is a
    // deleting destructor; the remaining methods are nonvirtual.
    class spDXMeshCombiner final
    {
    public:
        spDXMeshCombiner() noexcept = default;
        ~spDXMeshCombiner() = default;

        spDXMeshCombiner(const spDXMeshCombiner&) = delete;
        spDXMeshCombiner& operator=(const spDXMeshCombiner&) = delete;

        [[nodiscard]] bool IsFullForAnalysis() const noexcept;
        [[nodiscard]] bool InitializeForAnalysis(
            std::uint32_t fvfCode,
            std::uint32_t targetVertexCount,
            std::uint32_t vertexByteSize,
            std::uint32_t indexByteSize,
            std::uint32_t ignoredNativeArgument = 0);

        // Native callers copy into the two lock cursors before invoking the
        // counter-only commit at 0x004A98E0. The portable seam validates the
        // cursor/count advances atomically.
        [[nodiscard]] bool CommitForAnalysis(
            std::uint32_t vertexCount,
            std::uint32_t vertexByteCount,
            std::uint32_t indexCount,
            std::uint32_t indexByteCount) noexcept;

        [[nodiscard]] std::shared_ptr<spDXVertexBuffer>
            GetVertexBufferForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spDXIndexBuffer>
            GetIndexBufferForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetTargetVertexCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetWrittenVertexCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetWrittenIndexCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFVFCodeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexWriteOffsetForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetIndexWriteOffsetForAnalysis() const noexcept;
        [[nodiscard]] bool IsLockedForAnalysis() const noexcept;

    private:
        std::uint32_t targetVertexCount_ = 0;
        std::uint32_t writtenVertexCount_ = 0;
        std::uint32_t fvfCode_ = 0;
        std::uint32_t vertexByteSize_ = 0;
        std::uint32_t indexByteSize_ = 0;
        std::shared_ptr<spDXVertexBuffer> vertexBuffer_;
        std::shared_ptr<spDXIndexBuffer> indexBuffer_;
        std::uint32_t vertexWriteOffset_ = 0;
        std::uint32_t indexWriteOffset_ = 0;
        std::uint32_t writtenIndexCount_ = 0;
        bool locked_ = false;
    };

    // PC render-mesh implementation. Original factory4A9E80 was executed in
    // bounded guest evidence: allocation is exactly0x88. The class is absent from the PS2 binary,
    // which uses the separate spPS2Mesh backend.
    class spDXMesh final : public spRenderMesh
    {
    public:
        static constexpr spClassID ClassID = 0x193B2671;
        static constexpr std::uint32_t ExpandedPackedFieldBytes = 12;
        static constexpr std::size_t PackedFieldComponentOffsetIndex = 6;

        spDXMesh() noexcept = default;
        ~spDXMesh() override;

        spDXMesh(const spDXMesh&) = delete;
        spDXMesh& operator=(const spDXMesh&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Exact PC 0x004B21E0 mapping from engine vertex-component bits to
        // D3D9 FVF flags.
        [[nodiscard]] static std::uint32_t ComponentFlagsToFVFForAnalysis(
            std::uint32_t componentFlags) noexcept;
        // Native80 derives from bits02/04/08/10, NOT UV bits800..40000.
        // Analytical role: blend-weight count; original member name unknown.
        [[nodiscard]] static std::uint32_t ComponentWeightCountForAnalysis(
            std::uint32_t componentFlags) noexcept;

        // Portable form of the native secondary-interface initialization used
        // by the serializer hook. When combiner is non-null, the mesh receives
        // a range in its shared buffers; otherwise it owns standalone buffers
        // or CPU staging bytes depending on keepCPUData.
        [[nodiscard]] bool InitializeFromBuffersForAnalysis(
            const spIndexBuffer& indices,
            const spVertexBuffer& vertices,
            bool keepCPUData = false,
            spDXMeshCombiner* combiner = nullptr,
            spDXRenderer* renderer = nullptr);
        [[nodiscard]] bool InitializeFromMeshDataForAnalysis(
            const spMeshData& source,
            spDXMeshCombiner* combiner = nullptr);

        // Portable form of PC 0x004A9CC0 for a prebuilt shared buffer pair.
        [[nodiscard]] bool InitializeSharedForAnalysis(
            std::shared_ptr<spDXSharedMeshData> sharedData,
            std::uint32_t vertexComponentFlags,
            std::uint32_t fvfCode,
            spIndexBuffer::eIndexBufferType indexType,
            std::uint32_t indexBegin,
            std::uint32_t vertexBegin,
            std::uint32_t indexCount,
            std::uint32_t vertexCount,
            std::uint32_t vertexStride,
            const BoundingSphere* boundingSphere = nullptr);
        void ReleaseForAnalysis() noexcept;

        [[nodiscard]] spIndexBuffer::eIndexBufferType
            GetIndexTypeForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spDXIndexBuffer>
            GetDXIndexBufferForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spDXVertexBuffer>
            GetDXVertexBufferForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spDXSharedMeshData>
            GetSharedMeshDataForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<std::byte>&
            GetCPUIndexDataForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<std::byte>&
            GetCPUVertexDataForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetIndexByteSizeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexByteSizeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFVFCodeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexStrideForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetIndexBeginForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexBeginForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t
            GetComponentWeightCountForAnalysis() const noexcept;
        [[nodiscard]] std::shared_ptr<spPCVertexDeclaration>
            GetVertexDeclarationForAnalysis() const noexcept;

    private:
        [[nodiscard]] static bool BuildIndexBytesForAnalysis(
            const spIndexBuffer& source,
            std::vector<std::byte>& destination);
        [[nodiscard]] static bool BuildVertexBytesForAnalysis(
            const spVertexBuffer& source,
            std::vector<std::byte>& destination,
            std::uint32_t& destinationStride);
        [[nodiscard]] static std::uint32_t PrimitiveCountForAnalysis(
            spIndexBuffer::eIndexBufferType type,
            std::uint32_t indexCount) noexcept;

        spIndexBuffer::eIndexBufferType indexType_ =
            spIndexBuffer::eIndexBufferType::Type2;
        std::shared_ptr<spDXIndexBuffer> indexBuffer_;
        std::shared_ptr<spDXVertexBuffer> vertexBuffer_;
        std::shared_ptr<spDXSharedMeshData> sharedData_;
        std::uint32_t indexByteSize_ = 0;
        std::uint32_t vertexByteSize_ = 0;
        std::vector<std::byte> cpuIndexData_;
        std::vector<std::byte> cpuVertexData_;
        std::uint32_t fvfCode_ = 0;
        std::uint32_t vertexStride_ = 0;
        std::uint32_t indexBegin_ = 0;
        std::uint32_t vertexBegin_ = 0;
        std::uint32_t componentWeightCount_ = 0;
        // Native84 borrows renderer-owned object. Shared ownership is an
        // explicit host lifetime adaptation. Null without an explicit renderer
        // means CPU-buffer-only analysis, not a fully initialized PC backend.
        std::shared_ptr<spPCVertexDeclaration> vertexDeclaration_;
    };
}
