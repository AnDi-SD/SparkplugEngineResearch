#include "spSkinSerializer.h"

#include "spNode.h"
#include "spSkin.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSkinSerializer()
        {
            return std::make_unique<spSkinSerializer>();
        }

        const spRTTIRecord SkinSerializerRecord{
            spSkinSerializer::ClassID,
            spModelSerializer::ClassID,
            "spSkinSerializer",
            &spModelSerializer::StaticRTTI(),
            &CreateSkinSerializer,
            nullptr,
        };

        const bool SkinSerializerRegistered =
            spRTTIManager::Instance().Register(SkinSerializerRecord);
    }

    bool spSkinSerializer::PayloadPlanEntry::operator==(
        const PayloadPlanEntry& other) const noexcept
    {
        return segment == other.segment
            && elementCount == other.elementCount
            && byteCount == other.byteCount;
    }

    bool spSkinSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return model == other.model
            && skinFields == other.skinFields
            && payload == other.payload;
    }

    spSkinSerializer::~spSkinSerializer() = default;

    const spRTTIRecord& spSkinSerializer::StaticRTTI() noexcept
    {
        (void)SkinSerializerRegistered;
        return SkinSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSkinSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSkinSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spSkinSerializer::vfunc_18() const noexcept
    {
        return SkinSerializerRecord;
    }

    spClassID spSkinSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spSkin::ClassID;
    }

    spSkinSerializer::KnownWritePlan
    spSkinSerializer::BuildKnownWritePlanForAnalysis(const spSkin& skin) const
    {
        const auto boneCount = skin.GetBoneCountForAnalysis();
        return {
            spModelSerializer::BuildKnownWritePlanForAnalysis(skin),
            {Field::Skin},
            {
                {PayloadSegment::WeightCount, 1, sizeof(std::uint32_t)},
                {PayloadSegment::BoneCount, 1, sizeof(std::uint32_t)},
                {PayloadSegment::BoneRelationship, boneCount, 0},
                {PayloadSegment::InverseBindMatrix, boneCount,
                    boneCount * sizeof(spSkin::Matrix4)},
            },
        };
    }

    bool spSkinSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::Skin);
    }
}
