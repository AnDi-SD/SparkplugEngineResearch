#include "spDXMeshData.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXMeshData()
        {
            return std::make_unique<spDXMeshData>();
        }

        const spRTTIRecord DXMeshDataRecord{
            spDXMeshData::ClassID,
            spPlatformSpecificMeshData::ClassID,
            "spDXMeshData",
            &spPlatformSpecificMeshData::StaticRTTI(),
            &CreateDXMeshData,
            nullptr,
        };

        const bool DXMeshDataRegistered =
            spRTTIManager::Instance().Register(DXMeshDataRecord);
    }

    spDXMeshData::~spDXMeshData() = default;

    const spRTTIRecord& spDXMeshData::StaticRTTI() noexcept
    {
        (void)DXMeshDataRegistered;
        return DXMeshDataRecord;
    }

    std::unique_ptr<spBaseObject> spDXMeshData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXMeshData>();
        manager.RegisterClone(*this, *clone);

        // The native clone invokes only the inherited name-copy slot. Buffer
        // conversion is performed by a separate nonvirtual helper.
        return spPlatformSpecificMeshData::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXMeshData::vfunc_18() const noexcept
    {
        return DXMeshDataRecord;
    }

    bool spDXMeshData::InitializeFromMeshDataForAnalysis(
        const spMeshData& meshData)
    {
        const auto* const sourceIndices =
            meshData.GetIndexBufferForAnalysis();
        const auto* const sourceVertices =
            meshData.GetVertexBufferForAnalysis();
        if (sourceIndices == nullptr || sourceVertices == nullptr)
        {
            return false;
        }

        auto nextIndices = sourceIndices->CopyBufferForAnalysis();
        auto nextVertices = sourceVertices->CopyBufferForAnalysis();
        if (nextIndices == nullptr || nextVertices == nullptr)
        {
            return false;
        }

        indexBuffer_ = std::move(nextIndices);
        vertexBuffer_ = std::move(nextVertices);
        return true;
    }

    void spDXMeshData::ReleaseBuffersForAnalysis() noexcept
    {
        indexBuffer_.reset();
        vertexBuffer_.reset();
    }

    const spIndexBuffer* spDXMeshData::GetIndexBufferForAnalysis() const noexcept
    {
        return indexBuffer_.get();
    }

    const spVertexBuffer* spDXMeshData::GetVertexBufferForAnalysis() const noexcept
    {
        return vertexBuffer_.get();
    }
}
