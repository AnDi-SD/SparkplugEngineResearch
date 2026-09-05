#include "spRenderNodeSerializer.h"

#include "spRenderable.h"
#include "spRenderNode.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderNodeSerializer()
        {
            return std::make_unique<spRenderNodeSerializer>();
        }

        const spRTTIRecord RenderNodeSerializerRecord{
            spRenderNodeSerializer::ClassID,
            spNodeSerializer::ClassID,
            "spRenderNodeSerializer",
            &spNodeSerializer::StaticRTTI(),
            &CreateRenderNodeSerializer,
            nullptr,
        };

        const bool RenderNodeSerializerRegistered =
            spRTTIManager::Instance().Register(RenderNodeSerializerRecord);
    }

    spRenderNodeSerializer::~spRenderNodeSerializer() = default;

    const spRTTIRecord& spRenderNodeSerializer::StaticRTTI() noexcept
    {
        (void)RenderNodeSerializerRegistered;
        return RenderNodeSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spRenderNodeSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderNodeSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spRenderNodeSerializer::vfunc_18() const noexcept
    {
        return RenderNodeSerializerRecord;
    }

    spClassID spRenderNodeSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spRenderNode::ClassID;
    }

    spRenderNodeSerializer::WritePlanForAnalysis
    spRenderNodeSerializer::BuildKnownWritePlanForAnalysis(
        const spRenderNode& node) const
    {
        WritePlanForAnalysis plan;
        plan.nodeFields =
            spNodeSerializer::BuildKnownWritePlanForAnalysis(node);
        plan.renderables.reserve(node.GetRenderableCountForAnalysis());
        for (std::size_t index = 0;
             index < node.GetRenderableCountForAnalysis(); ++index)
        {
            if (const auto* renderable =
                    node.GetRenderableForAnalysis(index))
            {
                plan.renderables.push_back(renderable);
            }
        }
        return plan;
    }

    bool spRenderNodeSerializer::AttachResolvedRenderableForAnalysis(
        spRenderNode& node,
        std::shared_ptr<spBaseObject> relationship) const
    {
        auto renderable = std::dynamic_pointer_cast<spRenderable>(
            std::move(relationship));
        return renderable != nullptr
            && node.AttachRenderableForAnalysis(std::move(renderable));
    }
}
