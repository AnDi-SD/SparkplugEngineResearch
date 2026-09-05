// Exact original PC source path recovered from diagnostics:
//   Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp
// Reconstructed portions currently cover spDXSerializerHook, spDXMeshCombiner,
// spDXMesh and spDXMeshSerializer from this shared native translation unit.

#include "../Sparkplug/spSerializerHook.h"
#include "spDXCombinedVB.h"
#include "spDXMesh.h"
#include "spDXMeshSerializer.h"
#include "spDXIndexBuffer.h"
#include "spDXSharedMeshData.h"
#include "spDXVertexBuffer.h"

#include "../Sparkplug/spDataBlockSerializer.h"
#include "../Sparkplug/spMeshData.h"
#include "../Sparkplug/spResourceFATSerializer.h"
#include "../Sparkplug/spResourceManager.h"
#include "../Sparkplug/spVertexBuffer.h"

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXSerializerHook()
        {
            return std::make_unique<spDXSerializerHook>();
        }

        const spRTTIRecord DXSerializerHookRecord{
            spDXSerializerHook::ClassID,
            spSerializerHook::ClassID,
            "spDXSerializerHook",
            &spSerializerHook::StaticRTTI(),
            &CreateDXSerializerHook,
            nullptr,
        };

        const bool DXSerializerHookRegistered =
            spRTTIManager::Instance().Register(DXSerializerHookRecord);

        std::unique_ptr<spBaseObject> CreateDXMesh()
        {
            return std::make_unique<spDXMesh>();
        }

        const spRTTIRecord DXMeshRecord{
            spDXMesh::ClassID,
            spRenderMesh::ClassID,
            "spDXMesh",
            &spRenderMesh::StaticRTTI(),
            &CreateDXMesh,
            nullptr,
        };

        const bool DXMeshRegistered =
            spRTTIManager::Instance().Register(DXMeshRecord);

        std::unique_ptr<spBaseObject> CreateDXMeshSerializer()
        {
            return std::make_unique<spDXMeshSerializer>();
        }

        const spRTTIRecord DXMeshSerializerRecord{
            spDXMeshSerializer::ClassID,
            spSerializer::ClassID,
            "spDXMeshSerializer",
            &spSerializer::StaticRTTI(),
            &CreateDXMeshSerializer,
            nullptr,
        };

        const bool DXMeshSerializerRegistered =
            spRTTIManager::Instance().Register(DXMeshSerializerRecord);

        [[nodiscard]] bool AddWithoutOverflow(
            std::uint32_t& destination,
            const std::uint32_t value) noexcept
        {
            if (value > std::numeric_limits<std::uint32_t>::max() - destination)
            {
                return false;
            }
            destination += value;
            return true;
        }
    }

    spDXSerializerHook::~spDXSerializerHook() = default;

    const spRTTIRecord& spDXSerializerHook::StaticRTTI() noexcept
    {
        (void)DXSerializerHookRegistered;
        return DXSerializerHookRecord;
    }

    std::unique_ptr<spBaseObject> spDXSerializerHook::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXSerializerHook>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXSerializerHook::vfunc_18() const noexcept
    {
        return DXSerializerHookRecord;
    }

    void spDXSerializerHook::vfunc_24(
        spResourceFATHelperForAnalysis* const fatHelper,
        spStream& source)
    {
        lastBatchPlan_.clear();
        if (fatHelper == nullptr)
        {
            return;
        }

        // Native PC probes the resource cache before reading an unresolved
        // spMeshData payload. Reuse the already reconstructed common helper.
        if (const auto* const resources = spResourceManager::GetInstance();
            resources != nullptr)
        {
            (void)fatHelper->ResolveCachedResourcesForAnalysis(*resources);
        }
        (void)BuildBatchPlanForAnalysis(*fatHelper, source, lastBatchPlan_);
    }

    bool spDXSerializerHook::ReadDXMeshDataInfoForAnalysis(
        spStream& source,
        const spResourceFATEntryForAnalysis& entry,
        spDXMeshNativeInfoForAnalysis& info) noexcept
    {
        info = {};
        if (entry.offset > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max())
            || !source.Seek(spStream::SeekSource::essStart,
                static_cast<std::int32_t>(entry.offset))
            || !source.Read(info.objectClassID)
            || !source.Read(info.objectMarker))
        {
            return false;
        }

        spDataBlockSerializer blocks;
        for (;;)
        {
            const auto* const header = blocks.ReadHeaderForAnalysis(source);
            if (header == nullptr)
            {
                return false;
            }
            if (header->IsTerminator())
            {
                return true;
            }

            if (header->fieldID == 1)
            {
                constexpr std::uint32_t NativeInfoSize =
                    sizeof(std::uint32_t) * 4U + sizeof(std::uint8_t);
                if (header->payloadSize < NativeInfoSize)
                {
                    return false;
                }

                std::uint8_t indicesAre32Bit = 0;
                if (!source.Read(info.fvfCode)
                    || !source.Read(info.vertexCount)
                    || !source.Read(info.vertexDataSize)
                    || !source.Read(info.indexDataSize)
                    || !source.Read(indicesAre32Bit))
                {
                    return false;
                }
                info.indicesAre32Bit = indicesAre32Bit != 0;
                info.foundNativeField = true;

                // Native code rewinds to the payload start before SkipData.
                if (header->dataStreamPosition
                        > static_cast<std::uint32_t>(
                            std::numeric_limits<std::int32_t>::max())
                    || !source.Seek(spStream::SeekSource::essStart,
                        static_cast<std::int32_t>(
                            header->dataStreamPosition)))
                {
                    return false;
                }
            }

            if (!spDataBlockSerializer::SkipDataForAnalysis(source, *header))
            {
                return false;
            }
        }
    }

    bool spDXSerializerHook::BuildBatchPlanForAnalysis(
        spResourceFATHelperForAnalysis& fatHelper,
        spStream& source,
        std::vector<spDXMeshBatchForAnalysis>& plan) noexcept
    {
        struct Candidate final
        {
            const spResourceFATEntryForAnalysis* entry = nullptr;
            spDXMeshNativeInfoForAnalysis info{};
            bool assigned = false;
        };

        plan.clear();
        std::vector<Candidate> candidates;
        try
        {
            for (auto* entry = fatHelper.FirstForAnalysis(); entry != nullptr;
                 entry = fatHelper.NextForAnalysis())
            {
                if (entry->object != nullptr || entry->classID != spMeshData::ClassID)
                {
                    continue;
                }
                Candidate candidate;
                candidate.entry = entry;
                if (!ReadDXMeshDataInfoForAnalysis(source, *entry, candidate.info))
                {
                    plan.clear();
                    return false;
                }
                candidates.push_back(candidate);
            }

            for (;;)
            {
                std::size_t seed = candidates.size();
                for (std::size_t index = 0; index < candidates.size(); ++index)
                {
                    if (!candidates[index].assigned
                        && candidates[index].info.foundNativeField
                        && candidates[index].info.vertexCount
                            < MaximumCombinedVertexCount)
                    {
                        seed = index;
                        break;
                    }
                }
                if (seed == candidates.size())
                {
                    return true;
                }

                spDXMeshBatchForAnalysis batch;
                batch.fvfCode = candidates[seed].info.fvfCode;
                for (std::size_t index = seed; index < candidates.size(); ++index)
                {
                    auto& candidate = candidates[index];
                    if (candidate.assigned || !candidate.info.foundNativeField
                        || candidate.info.fvfCode != batch.fvfCode
                        || candidate.info.vertexCount
                            >= MaximumCombinedVertexCount - batch.vertexCount)
                    {
                        continue;
                    }
                    if (!AddWithoutOverflow(
                            batch.vertexDataSize,
                            candidate.info.vertexDataSize)
                        || !AddWithoutOverflow(
                            batch.indexDataSize,
                            candidate.info.indexDataSize))
                    {
                        plan.clear();
                        return false;
                    }
                    batch.vertexCount += candidate.info.vertexCount;
                    batch.resourceIDs.push_back(candidate.entry->id);
                    candidate.assigned = true;
                }
                plan.push_back(std::move(batch));
            }
        }
        catch (...)
        {
            // Native allocation failures are not recoverable at every point;
            // the portable analysis seam reports them without a partial plan.
            plan.clear();
            return false;
        }
    }

    const std::vector<spDXMeshBatchForAnalysis>&
    spDXSerializerHook::GetLastBatchPlanForAnalysis() const noexcept
    {
        return lastBatchPlan_;
    }

    bool spDXMeshCombiner::IsFullForAnalysis() const noexcept
    {
        // Native 0x004A95E0 uses equality, not a greater-or-equal test.
        return writtenVertexCount_ == targetVertexCount_;
    }

    bool spDXMeshCombiner::InitializeForAnalysis(
        const std::uint32_t fvfCode,
        const std::uint32_t targetVertexCount,
        const std::uint32_t vertexByteSize,
        const std::uint32_t indexByteSize,
        const std::uint32_t ignoredNativeArgument)
    {
        (void)ignoredNativeArgument;

        auto nextIndices = std::make_shared<spDXIndexBuffer>();
        if (!nextIndices->InitializeForAnalysis(
                indexByteSize, 8, 0x65, 1))
        {
            return false;
        }

        auto nextVertices = std::make_shared<spDXVertexBuffer>();
        if (!nextVertices->InitializeForAnalysis(
                vertexByteSize, 8, fvfCode, 1))
        {
            return false;
        }

        targetVertexCount_ = targetVertexCount;
        writtenVertexCount_ = 0;
        fvfCode_ = fvfCode;
        vertexByteSize_ = vertexByteSize;
        indexByteSize_ = indexByteSize;
        vertexBuffer_ = std::move(nextVertices);
        indexBuffer_ = std::move(nextIndices);
        vertexWriteOffset_ = 0;
        indexWriteOffset_ = 0;
        writtenIndexCount_ = 0;
        locked_ = true;
        return true;
    }

    bool spDXMeshCombiner::CommitForAnalysis(
        const std::uint32_t vertexCount,
        const std::uint32_t vertexByteCount,
        const std::uint32_t indexCount,
        const std::uint32_t indexByteCount) noexcept
    {
        if (!locked_ || vertexBuffer_ == nullptr || indexBuffer_ == nullptr
            || vertexCount > targetVertexCount_ - writtenVertexCount_
            || vertexByteCount > vertexByteSize_ - vertexWriteOffset_
            || indexByteCount > indexByteSize_ - indexWriteOffset_
            || indexCount
                > UINT32_MAX - writtenIndexCount_)
        {
            return false;
        }

        writtenVertexCount_ += vertexCount;
        vertexWriteOffset_ += vertexByteCount;
        writtenIndexCount_ += indexCount;
        indexWriteOffset_ += indexByteCount;
        if (IsFullForAnalysis())
        {
            // Native unlocks both underlying Direct3D buffers here.
            locked_ = false;
        }
        return true;
    }

    std::shared_ptr<spDXVertexBuffer>
    spDXMeshCombiner::GetVertexBufferForAnalysis() const noexcept
    {
        return vertexBuffer_;
    }

    std::shared_ptr<spDXIndexBuffer>
    spDXMeshCombiner::GetIndexBufferForAnalysis() const noexcept
    {
        return indexBuffer_;
    }

    std::uint32_t
    spDXMeshCombiner::GetTargetVertexCountForAnalysis() const noexcept
    {
        return targetVertexCount_;
    }

    std::uint32_t
    spDXMeshCombiner::GetWrittenVertexCountForAnalysis() const noexcept
    {
        return writtenVertexCount_;
    }

    std::uint32_t
    spDXMeshCombiner::GetWrittenIndexCountForAnalysis() const noexcept
    {
        return writtenIndexCount_;
    }

    std::uint32_t spDXMeshCombiner::GetFVFCodeForAnalysis() const noexcept
    {
        return fvfCode_;
    }

    std::uint32_t
    spDXMeshCombiner::GetVertexWriteOffsetForAnalysis() const noexcept
    {
        return vertexWriteOffset_;
    }

    std::uint32_t
    spDXMeshCombiner::GetIndexWriteOffsetForAnalysis() const noexcept
    {
        return indexWriteOffset_;
    }

    bool spDXMeshCombiner::IsLockedForAnalysis() const noexcept
    {
        return locked_;
    }

    spDXMesh::~spDXMesh()
    {
        ReleaseForAnalysis();
    }

    const spRTTIRecord& spDXMesh::StaticRTTI() noexcept
    {
        (void)DXMeshRegistered;
        return DXMeshRecord;
    }

    std::unique_ptr<spBaseObject> spDXMesh::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXMesh>();
        manager.RegisterClone(*this, *clone);

        // PC 0x004A9EE0 creates a fresh target and invokes the inherited copy
        // slot. GPU/shared/CPU payload fields therefore remain blank.
        return spRenderMesh::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXMesh::vfunc_18() const noexcept
    {
        return DXMeshRecord;
    }

    std::uint32_t spDXMesh::ComponentFlagsToFVFForAnalysis(
        const std::uint32_t componentFlags) noexcept
    {
        std::uint32_t fvf = (componentFlags & 0x40U) != 0 ? 0x12U : 0x02U;
        if ((componentFlags & 0x80U) != 0)
        {
            fvf |= 0x20U;
        }
        if ((componentFlags & 0x100U) != 0)
        {
            fvf |= 0x40U;
        }
        if ((componentFlags & 0x200U) != 0)
        {
            fvf |= 0x80U;
        }
        if ((componentFlags & 0x400U) != 0)
        {
            fvf |= 0x10100U;
        }

        if ((componentFlags & 0x40000U) != 0)
        {
            fvf |= 0x800U;
        }
        else if ((componentFlags & 0x20000U) != 0)
        {
            fvf |= 0x700U;
        }
        else if ((componentFlags & 0x10000U) != 0)
        {
            fvf |= 0x600U;
        }
        else if ((componentFlags & 0x8000U) != 0)
        {
            fvf |= 0x500U;
        }
        else if ((componentFlags & 0x4000U) != 0)
        {
            fvf |= 0x400U;
        }
        else if ((componentFlags & 0x2000U) != 0)
        {
            fvf |= 0x300U;
        }
        else if ((componentFlags & 0x1000U) != 0)
        {
            fvf |= 0x200U;
        }
        else if ((componentFlags & 0x800U) != 0)
        {
            fvf |= 0x100U;
        }

        if ((componentFlags & 0x10U) != 0)
        {
            fvf |= 0x0CU;
        }
        else if ((componentFlags & 0x08U) != 0)
        {
            fvf |= 0x0AU;
        }
        else if ((componentFlags & 0x04U) != 0)
        {
            fvf |= 0x08U;
        }
        else if ((componentFlags & 0x02U) != 0)
        {
            fvf |= 0x06U;
        }
        if ((componentFlags & 0x20U) != 0)
        {
            fvf |= 0x1000U;
        }
        return fvf;
    }

    std::uint32_t spDXMesh::TextureCoordinateCountForAnalysis(
        const std::uint32_t componentFlags) noexcept
    {
        if ((componentFlags & 0x10U) != 0)
        {
            return 4;
        }
        if ((componentFlags & 0x08U) != 0)
        {
            return 3;
        }
        if ((componentFlags & 0x04U) != 0)
        {
            return 2;
        }
        return (componentFlags >> 1U) & 1U;
    }

    bool spDXMesh::BuildIndexBytesForAnalysis(
        const spIndexBuffer& source,
        std::vector<std::byte>& destination)
    {
        if (!source.IsInitializedForAnalysis())
        {
            return false;
        }
        const auto elementSize = source.GetIndexElementSizeForAnalysis();
        const std::uint64_t byteSize =
            static_cast<std::uint64_t>(source.GetIndexCountForAnalysis())
            * elementSize;
        if (byteSize > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        try
        {
            destination.resize(static_cast<std::size_t>(byteSize));
        }
        catch (...)
        {
            return false;
        }
        for (std::uint32_t position = 0;
             position < source.GetIndexCountForAnalysis(); ++position)
        {
            const auto value = source.GetIndexForAnalysis(position);
            if (!value.has_value())
            {
                return false;
            }
            auto* const output = destination.data()
                + static_cast<std::size_t>(position) * elementSize;
            if (elementSize == sizeof(std::uint16_t))
            {
                if (*value > std::numeric_limits<std::uint16_t>::max())
                {
                    return false;
                }
                const auto narrow = static_cast<std::uint16_t>(*value);
                std::memcpy(output, &narrow, sizeof(narrow));
            }
            else
            {
                std::memcpy(output, &*value, sizeof(*value));
            }
        }
        return true;
    }

    bool spDXMesh::BuildVertexBytesForAnalysis(
        const spVertexBuffer& source,
        std::vector<std::byte>& destination,
        std::uint32_t& destinationStride)
    {
        if (!source.IsInitializedForAnalysis())
        {
            return false;
        }
        const auto sourceStride = source.GetVertexStrideForAnalysis();
        const auto vertexCount = source.GetVertexCountForAnalysis();
        const std::uint64_t sourceSize =
            static_cast<std::uint64_t>(sourceStride) * vertexCount;
        if (sourceSize > source.GetDataForAnalysis().size())
        {
            return false;
        }

        const bool expandsPackedField =
            (source.GetComponentFlagsForAnalysis() & 0x20U) != 0;
        destinationStride = sourceStride
            + (expandsPackedField ? ExpandedPackedFieldBytes : 0U);
        const std::uint64_t destinationSize =
            static_cast<std::uint64_t>(destinationStride) * vertexCount;
        if (destinationSize > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        try
        {
            destination.resize(static_cast<std::size_t>(destinationSize));
        }
        catch (...)
        {
            return false;
        }

        const auto& input = source.GetDataForAnalysis();
        if (!expandsPackedField)
        {
            std::memcpy(destination.data(), input.data(),
                static_cast<std::size_t>(sourceSize));
            return true;
        }

        const auto packedOffset = static_cast<std::size_t>(
            source.GetComponentOffsetsForAnalysis()[
                PackedFieldComponentOffsetIndex]) * sizeof(std::uint32_t);
        if (packedOffset > sourceStride || sourceStride - packedOffset < 4)
        {
            return false;
        }
        const auto tailSize = sourceStride - packedOffset - 4U;
        for (std::uint32_t vertex = 0; vertex < vertexCount; ++vertex)
        {
            const auto* const inputVertex = input.data()
                + static_cast<std::size_t>(vertex) * sourceStride;
            auto* const outputVertex = destination.data()
                + static_cast<std::size_t>(vertex) * destinationStride;
            std::memcpy(outputVertex, inputVertex, packedOffset);
            for (std::size_t component = 0; component < 4; ++component)
            {
                const float expanded = static_cast<float>(
                    std::to_integer<std::uint8_t>(
                        inputVertex[packedOffset + component]));
                std::memcpy(outputVertex + packedOffset
                        + component * sizeof(float),
                    &expanded, sizeof(expanded));
            }
            std::memcpy(outputVertex + packedOffset + 4U * sizeof(float),
                inputVertex + packedOffset + 4U, tailSize);
        }
        return true;
    }

    std::uint32_t spDXMesh::PrimitiveCountForAnalysis(
        const spIndexBuffer::eIndexBufferType type,
        const std::uint32_t indexCount) noexcept
    {
        switch (type)
        {
        case spIndexBuffer::eIndexBufferType::Type2:
            return indexCount / 3U;
        case spIndexBuffer::eIndexBufferType::Type3:
            return indexCount >= 2U ? indexCount - 2U : 0U;
        case spIndexBuffer::eIndexBufferType::Type4:
            return indexCount / 2U;
        default:
            // Native 0x004A9CC0 leaves the existing field untouched for type
            // 1 and unknown values. A fresh portable object starts at zero.
            return 0;
        }
    }

    bool spDXMesh::InitializeFromBuffersForAnalysis(
        const spIndexBuffer& indices,
        const spVertexBuffer& vertices,
        const bool keepCPUData,
        spDXMeshCombiner* const combiner)
    {
        std::vector<std::byte> nextIndices;
        std::vector<std::byte> nextVertices;
        std::uint32_t nextStride = 0;
        if (!BuildIndexBytesForAnalysis(indices, nextIndices)
            || !BuildVertexBytesForAnalysis(
                vertices, nextVertices, nextStride))
        {
            return false;
        }

        const auto fvf = ComponentFlagsToFVFForAnalysis(
            vertices.GetComponentFlagsForAnalysis());
        std::shared_ptr<spDXIndexBuffer> nextIndexBuffer;
        std::shared_ptr<spDXVertexBuffer> nextVertexBuffer;
        std::uint32_t nextIndexBegin = 0;
        std::uint32_t nextVertexBegin = 0;

        if (combiner != nullptr)
        {
            nextIndexBuffer = combiner->GetIndexBufferForAnalysis();
            nextVertexBuffer = combiner->GetVertexBufferForAnalysis();
            nextIndexBegin = combiner->GetWrittenIndexCountForAnalysis();
            nextVertexBegin = combiner->GetWrittenVertexCountForAnalysis();
            if (!combiner->IsLockedForAnalysis()
                || nextIndexBuffer == nullptr || nextVertexBuffer == nullptr
                || combiner->GetFVFCodeForAnalysis() != fvf
                || nextVertexBegin > combiner->GetTargetVertexCountForAnalysis()
                || vertices.GetVertexCountForAnalysis()
                    > combiner->GetTargetVertexCountForAnalysis()
                        - nextVertexBegin
                || combiner->GetIndexWriteOffsetForAnalysis()
                    > nextIndexBuffer->GetDataForAnalysis().size()
                || nextIndices.size()
                    > nextIndexBuffer->GetDataForAnalysis().size()
                        - combiner->GetIndexWriteOffsetForAnalysis()
                || combiner->GetVertexWriteOffsetForAnalysis()
                    > nextVertexBuffer->GetDataForAnalysis().size()
                || nextVertices.size()
                    > nextVertexBuffer->GetDataForAnalysis().size()
                        - combiner->GetVertexWriteOffsetForAnalysis())
            {
                return false;
            }

            std::memcpy(nextIndexBuffer->GetDataForAnalysis().data()
                    + combiner->GetIndexWriteOffsetForAnalysis(),
                nextIndices.data(), nextIndices.size());
            std::memcpy(nextVertexBuffer->GetDataForAnalysis().data()
                    + combiner->GetVertexWriteOffsetForAnalysis(),
                nextVertices.data(), nextVertices.size());
            if (!combiner->CommitForAnalysis(
                    vertices.GetVertexCountForAnalysis(),
                    static_cast<std::uint32_t>(nextVertices.size()),
                    indices.GetIndexCountForAnalysis(),
                    static_cast<std::uint32_t>(nextIndices.size())))
            {
                return false;
            }
        }
        else if (!keepCPUData)
        {
            try
            {
                nextIndexBuffer = std::make_shared<spDXIndexBuffer>();
                nextVertexBuffer = std::make_shared<spDXVertexBuffer>();
            }
            catch (...)
            {
                return false;
            }
            if (!nextIndexBuffer->InitializeForAnalysis(
                    static_cast<std::uint32_t>(nextIndices.size()), 8, 0x65, 1)
                || !nextVertexBuffer->InitializeForAnalysis(
                    static_cast<std::uint32_t>(nextVertices.size()),
                    8, fvf, 1))
            {
                return false;
            }
            nextIndexBuffer->GetDataForAnalysis() = nextIndices;
            nextVertexBuffer->GetDataForAnalysis() = nextVertices;
        }

        ReleaseForAnalysis();
        indexType_ = indices.GetTypeForAnalysis();
        indexBuffer_ = std::move(nextIndexBuffer);
        vertexBuffer_ = std::move(nextVertexBuffer);
        if (keepCPUData && combiner == nullptr)
        {
            cpuIndexData_ = std::move(nextIndices);
            cpuVertexData_ = std::move(nextVertices);
        }
        indexByteSize_ = static_cast<std::uint32_t>(
            indices.GetIndexCountForAnalysis()
            * indices.GetIndexElementSizeForAnalysis());
        vertexByteSize_ = nextStride * vertices.GetVertexCountForAnalysis();
        fvfCode_ = fvf;
        vertexStride_ = nextStride;
        indexBegin_ = nextIndexBegin;
        vertexBegin_ = nextVertexBegin;
        textureCoordinateCount_ = TextureCoordinateCountForAnalysis(
            vertices.GetComponentFlagsForAnalysis());
        SetMeshMetadataForAnalysis(
            vertices.GetComponentFlagsForAnalysis(),
            indices.GetPrimitiveCountForAnalysis(),
            vertices.GetVertexCountForAnalysis());
        MarkBoundsValidForAnalysis();
        return true;
    }

    bool spDXMesh::InitializeFromMeshDataForAnalysis(
        const spMeshData& source,
        spDXMeshCombiner* const combiner)
    {
        const auto* const indices = source.GetIndexBufferForAnalysis();
        const auto* const vertices = source.GetVertexBufferForAnalysis();
        return indices != nullptr && vertices != nullptr
            && InitializeFromBuffersForAnalysis(
                *indices, *vertices, false, combiner);
    }

    bool spDXMesh::InitializeSharedForAnalysis(
        std::shared_ptr<spDXSharedMeshData> sharedData,
        const std::uint32_t vertexComponentFlags,
        const std::uint32_t fvfCode,
        const spIndexBuffer::eIndexBufferType indexType,
        const std::uint32_t indexBegin,
        const std::uint32_t vertexBegin,
        const std::uint32_t indexCount,
        const std::uint32_t vertexCount,
        const std::uint32_t vertexStride,
        const BoundingSphere* const boundingSphere)
    {
        if (sharedData == nullptr)
        {
            return false;
        }
        const auto nextIndices = sharedData->GetIndexBufferForAnalysis();
        const auto nextVertices = sharedData->GetVertexBufferForAnalysis();
        const std::uint64_t indexEnd =
            (static_cast<std::uint64_t>(indexBegin) + indexCount) * 2U;
        const std::uint64_t vertexEnd =
            (static_cast<std::uint64_t>(vertexBegin) + vertexCount)
            * vertexStride;
        if (nextIndices == nullptr || nextVertices == nullptr
            || !nextIndices->IsInitializedForAnalysis()
            || !nextVertices->IsInitializedForAnalysis()
            || indexEnd > nextIndices->GetDataForAnalysis().size()
            || vertexEnd > nextVertices->GetDataForAnalysis().size())
        {
            return false;
        }

        ReleaseForAnalysis();
        indexType_ = indexType;
        indexBuffer_ = nextIndices;
        vertexBuffer_ = nextVertices;
        sharedData_ = std::move(sharedData);
        fvfCode_ = fvfCode;
        vertexStride_ = vertexStride;
        indexBegin_ = indexBegin;
        vertexBegin_ = vertexBegin;
        textureCoordinateCount_ =
            TextureCoordinateCountForAnalysis(vertexComponentFlags);
        SetMeshMetadataForAnalysis(vertexComponentFlags,
            PrimitiveCountForAnalysis(indexType, indexCount), vertexCount);
        if (boundingSphere != nullptr)
        {
            SetBoundingSphereForAnalysis(*boundingSphere);
        }
        MarkBoundsValidForAnalysis();
        return true;
    }

    void spDXMesh::ReleaseForAnalysis() noexcept
    {
        indexBuffer_.reset();
        vertexBuffer_.reset();
        sharedData_.reset();
        std::vector<std::byte>{}.swap(cpuIndexData_);
        std::vector<std::byte>{}.swap(cpuVertexData_);
        fvfCode_ = 0;
    }

    spIndexBuffer::eIndexBufferType
    spDXMesh::GetIndexTypeForAnalysis() const noexcept
    {
        return indexType_;
    }

    std::shared_ptr<spDXIndexBuffer>
    spDXMesh::GetDXIndexBufferForAnalysis() const noexcept
    {
        return indexBuffer_;
    }

    std::shared_ptr<spDXVertexBuffer>
    spDXMesh::GetDXVertexBufferForAnalysis() const noexcept
    {
        return vertexBuffer_;
    }

    std::shared_ptr<spDXSharedMeshData>
    spDXMesh::GetSharedMeshDataForAnalysis() const noexcept
    {
        return sharedData_;
    }

    const std::vector<std::byte>&
    spDXMesh::GetCPUIndexDataForAnalysis() const noexcept
    {
        return cpuIndexData_;
    }

    const std::vector<std::byte>&
    spDXMesh::GetCPUVertexDataForAnalysis() const noexcept
    {
        return cpuVertexData_;
    }

    std::uint32_t spDXMesh::GetIndexByteSizeForAnalysis() const noexcept
    {
        return indexByteSize_;
    }

    std::uint32_t spDXMesh::GetVertexByteSizeForAnalysis() const noexcept
    {
        return vertexByteSize_;
    }

    std::uint32_t spDXMesh::GetFVFCodeForAnalysis() const noexcept
    {
        return fvfCode_;
    }

    std::uint32_t spDXMesh::GetVertexStrideForAnalysis() const noexcept
    {
        return vertexStride_;
    }

    std::uint32_t spDXMesh::GetIndexBeginForAnalysis() const noexcept
    {
        return indexBegin_;
    }

    std::uint32_t spDXMesh::GetVertexBeginForAnalysis() const noexcept
    {
        return vertexBegin_;
    }

    std::uint32_t
    spDXMesh::GetTextureCoordinateCountForAnalysis() const noexcept
    {
        return textureCoordinateCount_;
    }

    std::uint32_t
    spDXMesh::GetRendererVertexFormatCodeForAnalysis() const noexcept
    {
        return rendererVertexFormatCode_;
    }

    spDXMeshSerializer::~spDXMeshSerializer() = default;

    const spRTTIRecord& spDXMeshSerializer::StaticRTTI() noexcept
    {
        (void)DXMeshSerializerRegistered;
        return DXMeshSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spDXMeshSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXMeshSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXMeshSerializer::vfunc_18() const noexcept
    {
        return DXMeshSerializerRecord;
    }

    spClassID spDXMeshSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    bool spDXMeshSerializer::WritePayloadForAnalysis(
        spStream& destination,
        const spDXMeshWirePayloadForAnalysis& payload,
        const spDXMeshRelationshipCodecForAnalysis& relationships) const
    {
        const std::array<float, 3> center{
            payload.boundingSphere[0],
            payload.boundingSphere[1],
            payload.boundingSphere[2],
        };
        return relationships.write != nullptr
            && destination.Write(payload.indexType)
            && destination.Write(payload.vertexComponentFlags)
            && destination.Write(payload.fvfCode)
            && relationships.write(
                relationships.context, destination, payload.relationship)
            && destination.Write(payload.indexBegin)
            && destination.Write(payload.vertexBegin)
            && destination.Write(payload.indexCount)
            && destination.Write(payload.vertexCount)
            && destination.Write(payload.vertexStride)
            && destination.Write(center)
            && destination.Write(payload.boundingSphere[3]);
    }

    bool spDXMeshSerializer::ReadPayloadForAnalysis(
        spStream& source,
        spDXMesh& target,
        const spDXMeshRelationshipCodecForAnalysis& relationships) const
    {
        spDXMeshWirePayloadForAnalysis payload{};
        std::array<float, 3> center{};
        if (relationships.read == nullptr
            || !source.Read(payload.indexType)
            || !source.Read(payload.vertexComponentFlags)
            || !source.Read(payload.fvfCode)
            || !relationships.read(
                relationships.context, source, payload.relationship)
            || !source.Read(payload.indexBegin)
            || !source.Read(payload.vertexBegin)
            || !source.Read(payload.indexCount)
            || !source.Read(payload.vertexCount)
            || !source.Read(payload.vertexStride)
            || !source.Read(center)
            || !source.Read(payload.boundingSphere[3]))
        {
            return false;
        }
        payload.boundingSphere[0] = center[0];
        payload.boundingSphere[1] = center[1];
        payload.boundingSphere[2] = center[2];
        auto sharedData = std::dynamic_pointer_cast<spDXSharedMeshData>(
            std::move(payload.relationship));
        return target.InitializeSharedForAnalysis(
            std::move(sharedData),
            payload.vertexComponentFlags,
            payload.fvfCode,
            static_cast<spIndexBuffer::eIndexBufferType>(payload.indexType),
            payload.indexBegin,
            payload.vertexBegin,
            payload.indexCount,
            payload.vertexCount,
            payload.vertexStride,
            &payload.boundingSphere);
    }

    bool spDXMeshSerializer::BuildPayloadForAnalysis(
        const spDXMesh& mesh,
        std::shared_ptr<spDXCombinedVB> rendererCombinedBuffer,
        spDXMeshWirePayloadForAnalysis& payload) noexcept
    {
        spDXCombinedVBRangeForAnalysis range{};
        if (rendererCombinedBuffer == nullptr
            || !rendererCombinedBuffer->GetMeshRangeForAnalysis(mesh, range))
        {
            return false;
        }

        payload.indexType = static_cast<std::uint8_t>(
            mesh.GetIndexTypeForAnalysis());
        payload.vertexComponentFlags =
            mesh.GetVertexComponentFlagsForAnalysis();
        payload.fvfCode = mesh.GetFVFCodeForAnalysis();
        payload.relationship = std::move(rendererCombinedBuffer);
        payload.indexBegin = range.indexBegin;
        payload.vertexBegin = range.vertexBegin;
        payload.indexCount = range.indexCount;
        payload.vertexCount = range.vertexCount;
        payload.vertexStride = range.vertexStride;
        payload.boundingSphere = mesh.GetBoundingSphereForAnalysis();
        return true;
    }
}
