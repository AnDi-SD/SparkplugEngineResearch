#pragma once

// Exact original translation-unit path recovered from the PC executable:
// Z:\Sparkplug\Code\Sparkplug\spRenderNodeSerializer.cpp

#include "spNodeSerializer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spRenderNode;
    class spRenderable;

    class spRenderNodeSerializer final : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x66EF6060;
        static constexpr spClassID TargetClassID = 0x603625D0;

        enum class Field : std::uint32_t
        {
            Renderable = 0,
        };

        struct WritePlanForAnalysis final
        {
            std::vector<spNodeSerializer::Field> nodeFields;
            std::vector<const spRenderable*> renderables;
        };

        spRenderNodeSerializer() noexcept = default;
        ~spRenderNodeSerializer() override;

        spRenderNodeSerializer(const spRenderNodeSerializer&) = delete;
        spRenderNodeSerializer& operator=(const spRenderNodeSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;

        // Native writer serializes the complete spNode section first, then
        // repeats field 0 once for every renderable in storage order.
        [[nodiscard]] WritePlanForAnalysis BuildKnownWritePlanForAnalysis(
            const spRenderNode& node) const;

        // Safe relationship-resolution seam for the confirmed reader action.
        // Native read requires spRenderable (0x4FDA4542), rejects null, and
        // immediately attaches the resolved object to the render node.
        [[nodiscard]] bool AttachResolvedRenderableForAnalysis(
            spRenderNode& node,
            std::shared_ptr<spBaseObject> relationship) const;
    private:
        bool WriteSectionsForAnalysis(spSerializerManager*,spStream&,const spRenderNode&,std::string*) const;
    };
}
