#include "spModelSerializer.h"

#include "spModel.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateModelSerializer()
        {
            return std::make_unique<spModelSerializer>();
        }

        const spRTTIRecord ModelSerializerRecord{
            spModelSerializer::ClassID,
            spRenderableSerializer::ClassID,
            "spModelSerializer",
            &spRenderableSerializer::StaticRTTI(),
            &CreateModelSerializer,
            nullptr,
        };

        const bool ModelSerializerRegistered =
            spRTTIManager::Instance().Register(ModelSerializerRecord);
    }

    bool spModelSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return renderableFields == other.renderableFields
            && modelFields == other.modelFields;
    }

    spModelSerializer::~spModelSerializer() = default;

    const spRTTIRecord& spModelSerializer::StaticRTTI() noexcept
    {
        (void)ModelSerializerRegistered;
        return ModelSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spModelSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spModelSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spModelSerializer::vfunc_18() const noexcept
    {
        return ModelSerializerRecord;
    }

    spClassID spModelSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spModel::ClassID;
    }

    spModelSerializer::KnownWritePlan
    spModelSerializer::BuildKnownWritePlanForAnalysis(const spModel& model) const
    {
        KnownWritePlan plan;
        plan.renderableFields =
            spRenderableSerializer::BuildKnownWritePlanForAnalysis(model);
        if (model.GetBaseMeshForAnalysis())
        {
            plan.modelFields.push_back(Field::Base);
        }
        plan.modelFields.push_back(Field::ProjectionGroup);
        return plan;
    }
}
