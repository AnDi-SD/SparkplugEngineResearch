#include "spRenderableSerializer.h"

#include "spRenderable.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderableSerializer()
        {
            return std::make_unique<spRenderableSerializer>();
        }

        const spRTTIRecord RenderableSerializerRecord{
            spRenderableSerializer::ClassID,
            spSerializer::ClassID,
            "spRenderableSerializer",
            &spSerializer::StaticRTTI(),
            &CreateRenderableSerializer,
            nullptr,
        };

        const bool RenderableSerializerRegistered =
            spRTTIManager::Instance().Register(RenderableSerializerRecord);
    }

    spRenderableSerializer::~spRenderableSerializer() = default;

    const spRTTIRecord& spRenderableSerializer::StaticRTTI() noexcept
    {
        (void)RenderableSerializerRegistered;
        return RenderableSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spRenderableSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderableSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spRenderableSerializer::vfunc_18() const noexcept
    {
        return RenderableSerializerRecord;
    }

    spClassID spRenderableSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spRenderable::ClassID;
    }

    std::vector<spRenderableSerializer::Field>
    spRenderableSerializer::BuildKnownWritePlanForAnalysis(
        const spRenderable& renderable) const
    {
        std::vector<Field> plan;
        if (renderable.GetMaterialForAnalysis())
        {
            plan.push_back(Field::Material);
        }
        if (renderable.GetFogForAnalysis())
        {
            plan.push_back(Field::Fog);
        }
        plan.push_back(Field::AlphaSortEnable);
        plan.push_back(Field::AlphaSortPriority);
        return plan;
    }
}
