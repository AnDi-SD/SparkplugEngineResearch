#include "spMeshData.h"

#include <cstring>
#include <utility>
#include <vector>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMeshData()
        {
            return std::make_unique<spMeshData>();
        }

        const spRTTIRecord MeshDataRecord{
            spMeshData::ClassID,
            spMesh::ClassID,
            "spMeshData",
            &spMesh::StaticRTTI(),
            &CreateMeshData,
            nullptr,
        };

        const bool MeshDataRegistered =
            spRTTIManager::Instance().Register(MeshDataRecord);
    }

    spMeshData::~spMeshData() = default;

    const spRTTIRecord& spMeshData::StaticRTTI() noexcept
    {
        (void)MeshDataRegistered;
        return MeshDataRecord;
    }

    std::unique_ptr<spBaseObject> spMeshData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMeshData>();
        manager.RegisterClone(*this, *clone);

        // Native RTTI clone invokes the inherited name-copy slot only. It is
        // intentionally distinct from CopyMeshDataForAnalysis().
        return spMesh::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMeshData::vfunc_18() const noexcept
    {
        return MeshDataRecord;
    }

    bool spMeshData::InitializeForAnalysis(
        const spIndexBuffer& indices,
        const spVertexBuffer& vertices)
    {
        if (!indices.IsInitializedForAnalysis()
            || !vertices.IsInitializedForAnalysis()
            || vertices.GetVertexStrideForAnalysis() < sizeof(Position))
        {
            return false;
        }

        const auto requiredBytes =
            static_cast<std::uint64_t>(vertices.GetVertexStrideForAnalysis())
            * vertices.GetVertexCountForAnalysis();
        if (requiredBytes > vertices.GetDataForAnalysis().size())
        {
            return false;
        }

        auto nextIndices = indices.CopyBufferForAnalysis();
        auto nextVertices = vertices.CopyBufferForAnalysis();
        if (nextIndices == nullptr || nextVertices == nullptr)
        {
            return false;
        }

        std::vector<Position> positions(vertices.GetVertexCountForAnalysis());
        const auto& bytes = vertices.GetDataForAnalysis();
        const auto stride = vertices.GetVertexStrideForAnalysis();
        for (std::size_t index = 0; index < positions.size(); ++index)
        {
            std::memcpy(
                positions[index].data(),
                bytes.data() + index * stride,
                sizeof(Position));
        }

        if (!ComputeBoundsForAnalysis(*nextIndices, positions))
        {
            return false;
        }
        indexBuffer_ = std::move(nextIndices);
        vertexBuffer_ = std::move(nextVertices);
        return true;
    }

    std::unique_ptr<spMeshData> spMeshData::CopyMeshDataForAnalysis() const
    {
        auto copy = std::make_unique<spMeshData>();
        if (indexBuffer_ == nullptr && vertexBuffer_ == nullptr)
        {
            return copy;
        }
        if (indexBuffer_ == nullptr || vertexBuffer_ == nullptr)
        {
            return nullptr;
        }

        copy->indexBuffer_ = indexBuffer_->CopyBufferForAnalysis();
        copy->vertexBuffer_ = vertexBuffer_->CopyBufferForAnalysis();
        if (copy->indexBuffer_ == nullptr || copy->vertexBuffer_ == nullptr)
        {
            return nullptr;
        }

        // Native deep-copy transfers the six min/max floats but not the
        // bounds-valid byte at +0x28.
        copy->minimum_ = minimum_;
        copy->maximum_ = maximum_;
        return copy;
    }

    void spMeshData::ReleaseBuffersForAnalysis() noexcept
    {
        indexBuffer_.reset();
        vertexBuffer_.reset();
        InvalidateBoundsForAnalysis();
    }

    const spIndexBuffer* spMeshData::GetIndexBufferForAnalysis() const noexcept
    {
        return indexBuffer_.get();
    }

    const spVertexBuffer* spMeshData::GetVertexBufferForAnalysis() const noexcept
    {
        return vertexBuffer_.get();
    }
}
