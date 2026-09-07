#include "spDXSharedMeshData.h"

#include "spDXIndexBuffer.h"
#include "spDXVertexBuffer.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXSharedMeshData()
        {
            return std::make_unique<spDXSharedMeshData>();
        }

        const spRTTIRecord DXSharedMeshDataRecord{
            spDXSharedMeshData::ClassID,
            spBaseObject::ClassID,
            "spDXSharedMeshData",
            &spBaseObject::StaticRTTI(),
            &CreateDXSharedMeshData,
            nullptr,
        };

        const bool DXSharedMeshDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXSharedMeshDataRecord);
    }

    spDXSharedMeshData::~spDXSharedMeshData() = default;

    const spRTTIRecord& spDXSharedMeshData::StaticRTTI() noexcept
    {
        (void)DXSharedMeshDataRegistered;
        return DXSharedMeshDataRecord;
    }

    std::unique_ptr<spBaseObject> spDXSharedMeshData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXSharedMeshData>();
        manager.RegisterClone(*this, *clone);

        // PC 0x004C2950 invokes only the inherited copy slot. The GPU
        // buffers are deliberately not copied by RTTI Clone().
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXSharedMeshData::vfunc_18() const noexcept
    {
        return DXSharedMeshDataRecord;
    }

    bool spDXSharedMeshData::InitializeForAnalysis(
        const std::vector<std::byte>& indexData,
        const std::vector<std::byte>& vertexData)
    {
        if (indexData.size() > std::numeric_limits<std::uint32_t>::max()
            || vertexData.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        try
        {
            auto nextIndices = std::make_shared<spDXIndexBuffer>();
            if (!nextIndices->InitializeForAnalysis(
                    static_cast<std::uint32_t>(indexData.size()), 8, 0x65, 1))
            {
                return false;
            }
            nextIndices->GetDataForAnalysis() = indexData;

            auto nextVertices = std::make_shared<spDXVertexBuffer>();
            if (!nextVertices->InitializeForAnalysis(
                    static_cast<std::uint32_t>(vertexData.size()), 8, 0, 1))
            {
                return false;
            }
            nextVertices->GetDataForAnalysis() = vertexData;

            indexBuffer_ = std::move(nextIndices);
            vertexBuffer_ = std::move(nextVertices);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void spDXSharedMeshData::ReleaseBuffersForAnalysis() noexcept
    {
        if (indexBuffer_ != nullptr)
        {
            indexBuffer_->ReleaseDeviceBufferForAnalysis();
        }
        if (vertexBuffer_ != nullptr)
        {
            vertexBuffer_->ReleaseDeviceBufferForAnalysis();
        }
        indexBuffer_.reset();
        vertexBuffer_.reset();
    }

    std::shared_ptr<spDXIndexBuffer>
    spDXSharedMeshData::GetIndexBufferForAnalysis() const noexcept
    {
        return indexBuffer_;
    }

    std::shared_ptr<spDXVertexBuffer>
    spDXSharedMeshData::GetVertexBufferForAnalysis() const noexcept
    {
        return vertexBuffer_;
    }
}
