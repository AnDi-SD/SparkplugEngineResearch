#pragma once

// The original implementation shares the exact PC translation-unit path
// Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp with spDXMesh. The class and
// all diagnostic strings are absent from the PS2 executable.

#include "../Sparkplug/spMesh.h"
#include "../Sparkplug/spSerializer.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spDXMesh;
    class spDXCombinedVB;
    class spDXSharedMeshData;
    class spStream;

    // Safe representation of the values surrounding the native serialized
    // relationship. The relationship's byte encoding belongs to the common
    // spSerializer graph protocol and is deliberately supplied by a codec.
    struct spDXMeshWirePayloadForAnalysis final
    {
        std::uint8_t indexType = 0;
        std::uint32_t vertexComponentFlags = 0;
        std::uint32_t fvfCode = 0;
        std::shared_ptr<spBaseObject> relationship;
        std::uint32_t indexBegin = 0;
        std::uint32_t vertexBegin = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t vertexStride = 0;
        spMesh::BoundingSphere boundingSphere{};
    };

    struct spDXMeshRelationshipCodecForAnalysis final
    {
        using Read = bool (*)(
            void* context,
            spStream& source,
            std::shared_ptr<spBaseObject>& relationship);
        using Write = bool (*)(
            void* context,
            spStream& destination,
            const std::shared_ptr<spBaseObject>& relationship);

        void* context = nullptr;
        Read read = nullptr;
        Write write = nullptr;
    };

    class spDXMeshSerializer final : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0xE712BCAD;
        static constexpr spClassID TargetClassID = 0x193B2671;
        static constexpr spClassID SharedMeshDataClassID = 0x293A2681;

        spDXMeshSerializer() noexcept = default;
        ~spDXMeshSerializer() override;

        spDXMeshSerializer(const spDXMeshSerializer&) = delete;
        spDXMeshSerializer& operator=(const spDXMeshSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

        // Exact scalar order around the opaque common relationship grammar:
        // u8 type, u32 components, u32 FVF, relationship, five u32 range
        // values, Vector3 center, float radius.
        [[nodiscard]] bool WritePayloadForAnalysis(
            spStream& destination,
            const spDXMeshWirePayloadForAnalysis& payload,
            const spDXMeshRelationshipCodecForAnalysis& relationships) const;
        [[nodiscard]] bool ReadPayloadForAnalysis(
            spStream& source,
            spDXMesh& target,
            const spDXMeshRelationshipCodecForAnalysis& relationships) const;

        // Builds the scalar view only when a caller has supplied the optimizer-
        // selected spDXCombinedVB source relationship and that aggregate holds
        // the mesh association returned by native helper 0x004C07E0. The five
        // values deliberately come from the aggregate, not the runtime mesh.
        [[nodiscard]] static bool BuildPayloadForAnalysis(
            const spDXMesh& mesh,
            std::shared_ptr<spDXCombinedVB> rendererCombinedBuffer,
            spDXMeshWirePayloadForAnalysis& payload) noexcept;
    };
}
