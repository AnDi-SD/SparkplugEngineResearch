#include "spMesh.h"

#include <algorithm>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord MeshRecord{
            spMesh::ClassID,
            spResource::ClassID,
            "spMesh",
            &spResource::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool MeshRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MeshRecord);
    }

    spMesh::~spMesh() = default;

    const spRTTIRecord& spMesh::StaticRTTI() noexcept
    {
        (void)MeshRegistered;
        return MeshRecord;
    }

    std::unique_ptr<spBaseObject> spMesh::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spMesh::vfunc_18() const noexcept
    {
        return MeshRecord;
    }

    bool spMesh::ComputeBoundsForAnalysis(
        const spIndexBuffer& indices,
        const std::vector<Position>& positions,
        const std::uint32_t vertexBegin) noexcept
    {
        if (!indices.IsInitializedForAnalysis())
        {
            return false;
        }

        const auto high = std::numeric_limits<float>::max();
        Position nextMinimum{high, high, high};
        Position nextMaximum{-high, -high, -high};
        for (std::uint32_t position = 0;
             position < indices.GetIndexCountForAnalysis(); ++position)
        {
            const auto index = indices.GetIndexForAnalysis(position);
            if (!index.has_value()
                || *index > std::numeric_limits<std::uint32_t>::max() - vertexBegin)
            {
                return false;
            }
            const auto resolved = static_cast<std::size_t>(*index + vertexBegin);
            if (resolved >= positions.size())
            {
                return false;
            }
            for (std::size_t component = 0; component < 3; ++component)
            {
                nextMinimum[component] =
                    std::min(nextMinimum[component], positions[resolved][component]);
                nextMaximum[component] =
                    std::max(nextMaximum[component], positions[resolved][component]);
            }
        }

        minimum_ = nextMinimum;
        maximum_ = nextMaximum;
        boundsValid_ = true;
        return true;
    }

    void spMesh::InvalidateBoundsForAnalysis() noexcept
    {
        boundsValid_ = false;
    }

    bool spMesh::HasBoundsForAnalysis() const noexcept
    {
        return boundsValid_;
    }

    const spMesh::Position& spMesh::GetMinimumForAnalysis() const noexcept
    {
        return minimum_;
    }

    const spMesh::Position& spMesh::GetMaximumForAnalysis() const noexcept
    {
        return maximum_;
    }

    const spMesh::BoundingSphere&
    spMesh::GetBoundingSphereForAnalysis() const noexcept
    {
        return boundingSphere_;
    }

    std::uint32_t
    spMesh::GetVertexComponentFlagsForAnalysis() const noexcept
    {
        return vertexComponentFlags_;
    }

    std::uint32_t spMesh::GetPrimitiveCountForAnalysis() const noexcept
    {
        return primitiveCount_;
    }

    std::uint32_t spMesh::GetVertexCountForAnalysis() const noexcept
    {
        return vertexCount_;
    }

    void spMesh::SetBoundingSphereForAnalysis(
        const BoundingSphere& sphere) noexcept
    {
        boundingSphere_ = sphere;
    }

    void spMesh::MarkBoundsValidForAnalysis() noexcept
    {
        boundsValid_ = true;
    }

    void spMesh::SetBoundsForAnalysis(const BoundingSphere& sphere,
        const Position& minimum, const Position& maximum) noexcept
    {
        boundingSphere_ = sphere;
        minimum_ = minimum;
        maximum_ = maximum;
        boundsValid_ = true;
    }

    void spMesh::SetMeshMetadataForAnalysis(
        const std::uint32_t componentFlags,
        const std::uint32_t primitiveCount,
        const std::uint32_t vertexCount) noexcept
    {
        vertexComponentFlags_ = componentFlags;
        primitiveCount_ = primitiveCount;
        vertexCount_ = vertexCount;
    }
}
