#include "spAnimTexControllerSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateAnimTexControllerSerializer()
        {
            return std::make_unique<spAnimTexControllerSerializer>();
        }

        const spRTTIRecord AnimTexControllerSerializerRecord{
            spAnimTexControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spAnimTexControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateAnimTexControllerSerializer,
            nullptr,
        };

        const bool AnimTexControllerSerializerRegistered =
            spRTTIManager::Instance().Register(AnimTexControllerSerializerRecord);
    }

    bool spAnimTexControllerSerializer::SegmentPlan::operator==(
        const SegmentPlan& other) const noexcept
    {
        return segment == other.segment
            && elementCount == other.elementCount
            && fixedByteCount == other.fixedByteCount
            && variableLength == other.variableLength;
    }

    spAnimTexControllerSerializer::~spAnimTexControllerSerializer() = default;

    const spRTTIRecord& spAnimTexControllerSerializer::StaticRTTI() noexcept
    {
        (void)AnimTexControllerSerializerRegistered;
        return AnimTexControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spAnimTexControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spAnimTexControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spAnimTexControllerSerializer::vfunc_18() const noexcept
    {
        return AnimTexControllerSerializerRecord;
    }

    spClassID
    spAnimTexControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spAnimTexControllerSerializer::Field>
    spAnimTexControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::ControllerBase};
    }

    spAnimTexControllerSerializer::PayloadPlan
    spAnimTexControllerSerializer::BuildPayloadPlanForAnalysis(
        const std::uint32_t frameCount) noexcept
    {
        return {{
            {PayloadSegment::FrameCount, 1, sizeof(std::uint32_t), false},
            {PayloadSegment::TimeArray, frameCount,
                static_cast<std::uint64_t>(frameCount) * sizeof(float), false},
            {PayloadSegment::TextureRelationships, frameCount, 0, true},
        }};
    }

    bool spAnimTexControllerSerializer::HasConsistentTrackShapeForAnalysis(
        const std::uint32_t timeCount,
        const std::uint32_t textureRelationshipCount) noexcept
    {
        return timeCount == textureRelationshipCount;
    }

    bool spAnimTexControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::ControllerBase);
    }
}
