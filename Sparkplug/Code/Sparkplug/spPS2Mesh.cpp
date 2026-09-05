#include "spPS2Mesh.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2Mesh()
        {
            return std::make_unique<spPS2Mesh>();
        }

        const spRTTIRecord PS2MeshRecord{
            spPS2Mesh::ClassID,
            spRenderMesh::ClassID,
            "spPS2Mesh",
            &spRenderMesh::StaticRTTI(),
            &CreatePS2Mesh,
            nullptr,
        };

        const bool PS2MeshRegistered =
            spRTTIManager::Instance().Register(PS2MeshRecord);
    }

    spPS2Mesh::~spPS2Mesh() = default;

    const spRTTIRecord& spPS2Mesh::StaticRTTI() noexcept
    {
        (void)PS2MeshRegistered;
        return PS2MeshRecord;
    }

    std::unique_ptr<spBaseObject> spPS2Mesh::vfunc_10(
        spCloneManager& manager) const
    {
        // Native 0x001EF530 creates a fresh leaf and registers the pair, but
        // the inherited copy slot is a success-only no-op. Prepared packet
        // data therefore intentionally does not cross the RTTI clone boundary.
        auto clone = std::make_unique<spPS2Mesh>();
        manager.RegisterClone(*this, *clone);
        return clone;
    }

    const spRTTIRecord& spPS2Mesh::vfunc_18() const noexcept
    {
        return PS2MeshRecord;
    }

    bool spPS2Mesh::AttachPreparedDataForAnalysis(
        std::unique_ptr<spPS2MeshData> data,
        const std::uint32_t primitiveCount,
        const std::uint32_t vertexCount) noexcept
    {
        if (!data)
        {
            return false;
        }

        preparedData_ = std::move(data);
        SetMeshMetadataForAnalysis(
            GetVertexComponentFlagsForAnalysis(), primitiveCount, vertexCount);
        return true;
    }

    void spPS2Mesh::ReleaseForAnalysis() noexcept
    {
        preparedData_.reset();
    }

    const spPS2MeshData* spPS2Mesh::GetPreparedDataForAnalysis() const noexcept
    {
        return preparedData_.get();
    }
}
