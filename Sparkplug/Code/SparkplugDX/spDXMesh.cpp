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
#include "spDXRenderer.h"

#include "../Sparkplug/spDataBlockSerializer.h"
#include "../Sparkplug/spMeshData.h"
#include "../Sparkplug/spMeshDataSerializer.h"
#include "../Sparkplug/spResourceFATSerializer.h"
#include "../Sparkplug/spResourceManager.h"
#include "../Sparkplug/spSerializerManager.h"
#include "../Sparkplug/spVertexBuffer.h"
#include "Analysis/PC/spVertexBounds.h"

#include <cstring>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace sparkplug::reconstruction
{
    namespace
    {
        struct PCMeshBounds
        {
            spMesh::BoundingSphere sphere{};
            spMesh::Position minimum{}, maximum{};
        };

        // PC424230 -> 468370/468000. Sphere scans every vertex; the separate
        // AABB scans primitiveCount uint16 words even for a uint32 index stream.
        // Keep the older decoded-position analytical helper independent.
        bool ComputePCMeshBounds(const spIndexBuffer& indices,
            const spVertexBuffer& vertices, const std::vector<std::byte>& indexBytes,
            PCMeshBounds& result) noexcept
        {
            const auto& bytes = vertices.GetDataForAnalysis();
            const std::size_t stride = vertices.GetVertexStrideForAnalysis();
            const std::size_t count = vertices.GetVertexCountForAnalysis();
            if (stride < 12 || count > bytes.size() / stride
                || indices.GetPrimitiveCountForAnalysis() > indexBytes.size() / 2)
                return false;
            const auto position = [&](std::size_t index) {
                spMesh::Position value{};
                std::memcpy(value.data(), bytes.data() + index * stride, 12);
                return value;
            };
            const float high = std::numeric_limits<float>::max();
            if(!evidence::pc::ComputeVertexSphere(vertices,result.sphere))return false;
            result.minimum = {high, high, high}; result.maximum = {-high, -high, -high};
            for (std::size_t i = 0; i < indices.GetPrimitiveCountForAnalysis(); ++i)
            {
                std::uint16_t index = 0;
                std::memcpy(&index, indexBytes.data() + i * 2, 2);
                if (index >= count) return false; // native has no range guard
                const auto v = position(index);
                for (std::size_t c = 0; c < 3; ++c)
                {
                    if (v[c] < result.minimum[c]) result.minimum[c] = v[c];
                    if (v[c] > result.maximum[c]) result.maximum[c] = v[c];
                }
            }
            return true;
        }

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXSerializerHookRecord);

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXMeshRecord);

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXMeshSerializerRecord);

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
        const auto* manager = spSerializerManager::GetInstance();
        // Missing host manager does not create an implicit process-owned
        // singleton. Native would lazily create one with default platform0.
        (void)PrepareForAnalysis(manager ? manager->GetPlatformMaskForAnalysis() : 0,
            spResourceManager::GetInstance(), fatHelper, source);
    }

    bool spDXSerializerHook::PrepareForAnalysis(std::uint32_t platformMask,
        const spResourceManager* resources, spResourceFATHelperForAnalysis* fatHelper,
        spStream& source)
    {
        lastBatchPlan_.clear();
        if ((platformMask & spSerializerManager::PlatformPC) == 0) return true;
        if (fatHelper == nullptr) return false;

        // Native PC probes the resource cache before reading an unresolved
        // spMeshData payload. Reuse the already reconstructed common helper.
        if (resources != nullptr)
        {
            for (auto* entry = fatHelper->FirstForAnalysis(); entry; entry = fatHelper->NextForAnalysis())
                if (!entry->object && entry->classID == spMeshData::ClassID)
                    entry->object = resources->FindForAnalysis(entry->classID, entry->GetNameForAnalysis());
        }
        return BuildBatchPlanForAnalysis(*fatHelper, source, lastBatchPlan_);
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

    bool spDXSerializerHook::MaterializePreparedForAnalysis(spStream& source,
        spSerializerReadContextForAnalysis& context,std::string* error)
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){context.failed=true;if(error&&error->empty())*error=message;return false;};
        if(context.failed||context.depth||context.activeMeshCombiner)return fail("Invalid mesh batch context");
        if(lastBatchPlan_.empty())return true;
        if(!context.pcRenderer)return fail("PC mesh batches require an explicit analytical renderer");
        auto* fat=context.manager.GetFATForAnalysis();
        std::uint32_t size=0;
        if(!fat||!source.GetSize(&size)||source.GetLogicalOriginForAnalysis()>size)return fail("Invalid mesh batch stream");
        size-=source.GetLogicalOriginForAnalysis();
        std::uint64_t budget=0;
        try
        {
            for(const auto& batch:lastBatchPlan_)
            {
                budget+=std::uint64_t(batch.vertexDataSize)+batch.indexDataSize;
                if(!batch.vertexCount||budget>2ull*spMeshDataSerializer::MaximumPayloadBytesForAnalysis)
                    return fail("Mesh batch allocation budget exceeded or empty batch");
                spDXMeshCombiner combiner;
                if(!combiner.InitializeForAnalysis(batch.fvfCode,batch.vertexCount,batch.vertexDataSize,batch.indexDataSize))
                    return fail("Cannot initialize bounded mesh combiner");
                for(auto id:batch.resourceIDs)
                {
                    auto* entry=fat->FindByIDForAnalysis(id);
                    if(!entry||entry->object||entry->fileID||entry->classID!=spMeshData::ClassID)
                        return fail("Prepared mesh entry changed or is external");
                    if(entry->offset>size||entry->size<8||entry->size>size-entry->offset||
                        entry->offset>std::uint32_t(std::numeric_limits<std::int32_t>::max())||
                        !source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(entry->offset)))
                        return fail("Invalid mesh FAT extent or seek failure");
                    auto* serializer=context.manager.FindForAnalysis(entry->classID);
                    if(!serializer||context.GetCreatedObjectCountForAnalysis()>=context.maximumCreatedObjectsForAnalysis)
                        return fail("Missing mesh serializer or object budget exceeded");
                    const auto payloadTrace=context.BeginPayloadReadTraceForAnalysis(entry->id,entry->classID,source,entry->size,
                        spSerializerReadContextForAnalysis::PayloadReadKindForAnalysis::PreparedMesh);
                    auto object=serializer->ReadObjectHeaderAndCreateForAnalysis(source);
                    if(!object)return fail("Mesh header/factory failed");
                    auto* pointer=context.PublishObjectForAnalysis(std::move(object));
                    bool loaded=false;
                    {
                        struct BatchScope final
                        {
                            spSerializerReadContextForAnalysis& context;
                            const std::uint32_t previousConsumer;
                            BatchScope(spSerializerReadContextForAnalysis& c,spDXMeshCombiner& b,std::uint32_t id)
                                :context(c),previousConsumer(c.currentFileReadObjectIdForAnalysis)
                            {context.activeMeshCombiner=&b;++context.depth;context.currentFileReadObjectIdForAnalysis=id;}
                            ~BatchScope(){--context.depth;context.activeMeshCombiner=nullptr;context.currentFileReadObjectIdForAnalysis=previousConsumer;}
                        } scope(context,combiner,entry->id);
                        loaded=serializer->ReadPayloadForAnalysis(context,source,entry->size-8,*pointer,error);
                    }
                    std::uint32_t end=0;
                    if(!loaded||context.failed||!source.GetCurrentPosition(end)||end!=entry->offset+entry->size)
                        return fail("Mesh payload failed or did not consume its exact extent");
                    context.CompletePayloadReadTraceForAnalysis(payloadTrace,true);
                    // Unlike generic ReadReference/outer materialization,
                    // original4AA870 publishes only AFTER successful payload.
                    entry->object=pointer;
                    if(pointer->IsKindOf(spNamedObject::ClassID))
                        if(auto* named=dynamic_cast<spNamedObject*>(pointer))named->SetName(entry->GetNameForAnalysis());
                }
                if(!combiner.IsFullForAnalysis())return fail("Mesh batch did not reach declared vertex count");
                // Host owns this temporary: destructor drops its wrapper refs.
                // Original hook leaves the combiner allocated (documented).
            }
        }
        catch(...){return fail("Host mesh batch allocation failed");}
        return true;
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

    std::uint32_t spDXMesh::ComponentWeightCountForAnalysis(
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
        const auto& bytes = source.GetDataForAnalysis();
        return BuildPCDXVertexBytesForAnalysis(bytes.data(), bytes.size(),
            source.GetVertexStrideForAnalysis(), source.GetVertexCountForAnalysis(),
            source.GetComponentFlagsForAnalysis(),
            source.GetComponentOffsetsForAnalysis()[PackedFieldComponentOffsetIndex],
            destination, destinationStride);
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
        spDXMeshCombiner* const combiner,
        spDXRenderer* const renderer)
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

        PCMeshBounds nextBounds;
        if (!ComputePCMeshBounds(indices, vertices, nextIndices, nextBounds))
            return false;

        const auto fvf = ComponentFlagsToFVFForAnalysis(
            vertices.GetComponentFlagsForAnalysis());
        auto nextDeclaration=renderer
            ? renderer->GetVertexDeclarationForAnalysis(vertices.GetComponentFlagsForAnalysis())
            : nullptr;
        if(renderer&&!nextDeclaration)return false;
        std::shared_ptr<spDXIndexBuffer> nextIndexBuffer;
        std::shared_ptr<spDXVertexBuffer> nextVertexBuffer;
        std::uint32_t nextIndexBegin = 0;
        std::uint32_t nextVertexBegin = 0;

        if (combiner != nullptr)
        {
            // Native planning header may contain engine mask940, whereas this
            // mesh FVF is152 (unchanged logo_screen.smo). Do not equate those
            // two domains. Capacity/range validation below remains strict.
            nextIndexBuffer = combiner->GetIndexBufferForAnalysis();
            nextVertexBuffer = combiner->GetVertexBufferForAnalysis();
            nextIndexBegin = combiner->GetWrittenIndexCountForAnalysis();
            nextVertexBegin = combiner->GetWrittenVertexCountForAnalysis();
            if (!combiner->IsLockedForAnalysis()
                || nextIndexBuffer == nullptr || nextVertexBuffer == nullptr
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
        vertexDeclaration_=std::move(nextDeclaration);
        vertexStride_ = nextStride;
        indexBegin_ = nextIndexBegin;
        vertexBegin_ = nextVertexBegin;
        componentWeightCount_ = ComponentWeightCountForAnalysis(
            vertices.GetComponentFlagsForAnalysis());
        SetMeshMetadataForAnalysis(
            vertices.GetComponentFlagsForAnalysis(),
            indices.GetPrimitiveCountForAnalysis(),
            vertices.GetVertexCountForAnalysis());
        SetBoundsForAnalysis(nextBounds.sphere, nextBounds.minimum, nextBounds.maximum);
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
        componentWeightCount_ =
            ComponentWeightCountForAnalysis(vertexComponentFlags);
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
        vertexDeclaration_.reset();
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
    spDXMesh::GetComponentWeightCountForAnalysis() const noexcept
    {
        return componentWeightCount_;
    }

    std::shared_ptr<spPCVertexDeclaration>
    spDXMesh::GetVertexDeclarationForAnalysis() const noexcept
    {
        return vertexDeclaration_;
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
