#include "spPS2MeshData.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2MeshData()
        {
            return std::make_unique<spPS2MeshData>();
        }

        const spRTTIRecord PS2MeshDataRecord{
            spPS2MeshData::ClassID,
            spPlatformSpecificMeshData::ClassID,
            "spPS2MeshData",
            &spPlatformSpecificMeshData::StaticRTTI(),
            &CreatePS2MeshData,
            nullptr,
        };

        const bool PS2MeshDataRegistered =
            spRTTIManager::Instance().Register(PS2MeshDataRecord);

        std::uint32_t CountSetBits(
            std::uint32_t value,
            unsigned firstBit,
            unsigned lastBit) noexcept
        {
            std::uint32_t result = 0;
            for (unsigned bit = firstBit; bit <= lastBit; ++bit)
            {
                result += (value >> bit) & 1U;
            }
            return result;
        }

        std::uint32_t SelectField18(std::uint32_t flags) noexcept
        {
            const bool field36 = (flags & 0x000100U) != 0;
            const bool field38 = (flags & 0x000200U) != 0;
            const bool field32 = (flags & 0x000040U) != 0;
            const bool field3C = (flags & 0x000800U) != 0;

            if (field38)
            {
                return 8;
            }
            if (field36)
            {
                if (!field3C)
                {
                    return 5;
                }
                return field32 ? 7U : 3U;
            }
            if (field3C)
            {
                return field32 ? 6U : 2U;
            }
            return field32 ? 4U : 0U;
        }

        std::uint32_t CountField20(std::uint32_t flags) noexcept
        {
            std::uint32_t result = 1;
            result += (flags & 0x000100U) != 0 ? 1U : 0U;
            result += (flags & 0x000200U) != 0 ? 1U : 0U;
            result += (flags & 0x000040U) != 0 ? 1U : 0U;
            result += CountSetBits(flags, 11, 18);
            result += (flags & 0x00001EU) != 0 ? 1U : 0U;
            result += (flags & 0x000020U) != 0 ? 1U : 0U;
            return result;
        }
    }

    spPS2MeshData::spPS2MeshData() noexcept
    {
        field48_.fill(0xFFFFFFFFU);
        fieldA0_.fill(-1);

        field48_[0] = 0x0001006CU;
        for (std::size_t index = 2; index <= 6; ++index)
        {
            field48_[index] = 0x0001006CU;
        }
        field48_[7] = 0x0001006FU;
        field48_[9] = 0x0000006EU;
        for (std::size_t index = 12; index <= 19; ++index)
        {
            field48_[index] = 0x00010064U;
        }

        for (std::size_t index = 0; index <= 7; ++index)
        {
            fieldA0_[index] = static_cast<std::int32_t>(12 + index);
        }
        fieldA0_[8] = 9;
        fieldA0_[9] = 7;
        fieldA0_[10] = 0;
    }

    spPS2MeshData::~spPS2MeshData() = default;

    const spRTTIRecord& spPS2MeshData::StaticRTTI() noexcept
    {
        (void)PS2MeshDataRegistered;
        return PS2MeshDataRecord;
    }

    std::unique_ptr<spBaseObject> spPS2MeshData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2MeshData>();
        manager.RegisterClone(*this, *clone);

        // Native clone constructs a fresh 0x100-byte object and invokes only
        // the inherited name-copy slot. Runtime packet state remains default.
        return spPlatformSpecificMeshData::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2MeshData::vfunc_18() const noexcept
    {
        return PS2MeshDataRecord;
    }

    spPS2MeshData::PreparationPlan
        spPS2MeshData::PlanPreparationForAnalysis(
            const spIndexBuffer& indices,
            const spVertexBuffer& vertices) noexcept
    {
        PreparationPlan result;
        const auto type = indices.GetTypeForAnalysis();
        if (type == spIndexBuffer::eIndexBufferType::Type4)
        {
            result.accepted = true;
            result.alreadyPrepared = true;
            return result;
        }
        if (type != spIndexBuffer::eIndexBufferType::Type2
            && type != spIndexBuffer::eIndexBufferType::Type3)
        {
            return result;
        }

        result.accepted = true;
        result.sourceComponentFlags =
            vertices.GetComponentFlagsForAnalysis();
        result.effectiveComponentFlags = result.sourceComponentFlags;

        const bool hasField3C =
            (result.sourceComponentFlags & 0x000800U) != 0;
        const bool hasField32Or36 =
            (result.sourceComponentFlags & (0x000040U | 0x000100U)) != 0;
        result.requiresExpandedVertexBuffer =
            !(hasField3C && hasField32Or36);
        if (!hasField3C)
        {
            result.effectiveComponentFlags |= 0x000800U;
        }
        if (!hasField32Or36)
        {
            result.effectiveComponentFlags |= 0x000100U;
        }

        result.field14 = type == spIndexBuffer::eIndexBufferType::Type2
            ? 3U
            : 4U;
        result.field18 = SelectField18(result.effectiveComponentFlags);
        result.field20 = CountField20(result.effectiveComponentFlags);
        result.field38 = CountSetBits(result.effectiveComponentFlags, 12, 18);
        result.field3C = CountSetBits(result.effectiveComponentFlags, 1, 4);
        result.sourceIndexCount = indices.GetIndexCountForAnalysis();
        return result;
    }

    std::uint32_t spPS2MeshData::GetField14ForAnalysis() const noexcept
    {
        return field14_;
    }

    const std::array<std::uint32_t, spPS2MeshData::FieldTableCount>&
        spPS2MeshData::GetField48ForAnalysis() const noexcept
    {
        return field48_;
    }

    const std::array<std::int32_t, spPS2MeshData::FieldTableCount>&
        spPS2MeshData::GetFieldA0ForAnalysis() const noexcept
    {
        return fieldA0_;
    }

    std::uint32_t spPS2MeshData::GetFieldF8ForAnalysis() const noexcept
    {
        return fieldF8_;
    }

    bool spPS2MeshData::HasPreparedPacketForAnalysis() const noexcept
    {
        return hasPreparedPacket_;
    }

    bool spPS2MeshData::HasExternalPacketForAnalysis() const noexcept
    {
        return externalPacket_;
    }
}
