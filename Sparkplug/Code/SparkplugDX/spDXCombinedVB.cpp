#include "spDXCombinedVB.h"

#include "spDXMesh.h"

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXCombinedVB()
        {
            return std::make_unique<spDXCombinedVB>();
        }

        const spRTTIRecord DXCombinedVBRecord{
            spDXCombinedVB::ClassID,
            spBaseObject::ClassID,
            "spDXCombinedVB",
            &spBaseObject::StaticRTTI(),
            &CreateDXCombinedVB,
            nullptr,
        };

        const bool DXCombinedVBRegistered =
            spRTTIManager::Instance().Register(DXCombinedVBRecord);
    }

    spDXCombinedVB::~spDXCombinedVB() = default;

    const spRTTIRecord& spDXCombinedVB::StaticRTTI() noexcept
    {
        (void)DXCombinedVBRegistered;
        return DXCombinedVBRecord;
    }

    std::unique_ptr<spBaseObject> spDXCombinedVB::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXCombinedVB>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXCombinedVB::vfunc_18() const noexcept
    {
        return DXCombinedVBRecord;
    }

    bool spDXCombinedVB::InitializePayloadForAnalysis(
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
            auto nextIndices = indexData;
            auto nextVertices = vertexData;
            indexData_.swap(nextIndices);
            vertexData_.swap(nextVertices);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void spDXCombinedVB::ReleasePayloadForAnalysis() noexcept
    {
        std::vector<std::byte>{}.swap(indexData_);
        std::vector<std::byte>{}.swap(vertexData_);
    }

    const std::vector<std::byte>&
    spDXCombinedVB::GetIndexDataForAnalysis() const noexcept
    {
        return indexData_;
    }

    const std::vector<std::byte>&
    spDXCombinedVB::GetVertexDataForAnalysis() const noexcept
    {
        return vertexData_;
    }

    std::uint32_t spDXCombinedVB::GetIndexByteSizeForAnalysis() const noexcept
    {
        return static_cast<std::uint32_t>(indexData_.size());
    }

    std::uint32_t spDXCombinedVB::GetVertexByteSizeForAnalysis() const noexcept
    {
        return static_cast<std::uint32_t>(vertexData_.size());
    }

    bool spDXCombinedVB::RegisterMeshRangeForAnalysis(
        const spDXMesh& mesh,
        const spDXCombinedVBRangeForAnalysis& range)
    {
        const auto indexEnd = static_cast<std::uint64_t>(range.indexBegin)
            + range.indexCount;
        const auto vertexByteEnd =
            (static_cast<std::uint64_t>(range.vertexBegin)
                + range.vertexCount)
            * range.vertexStride;
        if (indexEnd * sizeof(std::uint16_t) > indexData_.size()
            || vertexByteEnd > vertexData_.size()
            || (range.vertexCount != 0 && range.vertexStride == 0))
        {
            return false;
        }
        try
        {
            meshRanges_.insert_or_assign(&mesh, range);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool spDXCombinedVB::GetMeshRangeForAnalysis(
        const spDXMesh& mesh,
        spDXCombinedVBRangeForAnalysis& range) const noexcept
    {
        const auto found = meshRanges_.find(&mesh);
        if (found == meshRanges_.end())
        {
            return false;
        }
        range = found->second;
        return true;
    }

    std::size_t spDXCombinedVB::GetMeshRangeCountForAnalysis() const noexcept
    {
        return meshRanges_.size();
    }

    void spDXCombinedVB::ClearMeshRangesForAnalysis() noexcept
    {
        meshRanges_.clear();
    }
}
