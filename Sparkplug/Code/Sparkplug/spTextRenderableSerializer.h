#pragma once

// Inferred source path. PC441C10 supplies the own-field read slice; its
// setters also run layout. Runtime and metadata share one field reader.
#include "spRenderable.h"
#include "spRenderableSerializer.h"

namespace sparkplug::reconstruction
{
    class spTextRenderableSerializer final : public spRenderableSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x15E97F32;
        static constexpr spClassID TargetClassID = 0x19A745D7;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        // Clone/write remain explicit host refusals.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;
        [[nodiscard]] std::unique_ptr<spBaseObject> ReadObjectHeaderAndCreateForAnalysis(
            spStream&, spSerializerObjectHeaderForAnalysis* = nullptr) const override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&, spStream&,
            std::uint32_t, spBaseObject&, std::string*) const override;
        bool WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&, spStream&,
            const spBaseObject&, std::string*) const override;
        bool IndexRelationshipsForAnalysis(spBaseObject&) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&, spBaseObject&) const override;

        struct InspectionForAnalysis final
        {
            // Only inherited Renderable scalar assignments use an actual object.
            // Material, fog and font remain unresolved wire observations.
            spRenderable partial;
            spRenderableSerializer::InspectionForAnalysis renderable;
            std::string text;
            bool textWasNull = true;
            // ReadString removes one trailing NUL. Together these observations
            // preserve all wire bytes without selecting a display encoding.
            bool textHadTrailingNull = false;
            std::uint16_t textByteCount = 0;
            std::uint32_t color = 0xFFFFFFFFu;
            std::uint32_t wrapWidth = 0;
            std::uint32_t alignment = 0;
            std::uint32_t fieldMask = 0;
            std::optional<evidence::pc::serialization::InspectedReference> font;
            static constexpr bool derivedLayoutAvailable = false;

            InspectionForAnalysis() = default;
            // Inherited observations borrow partial; its address must stay stable.
            InspectionForAnalysis(const InspectionForAnalysis&) = delete;
            InspectionForAnalysis& operator=(const InspectionForAnalysis&) = delete;
        };

        // Resets observations to factory defaults, then inspects inherited and
        // own sections. Consume only after true. No text setter, font assignment
        // or layout was executed; this never returns a loaded TextRenderable.
        [[nodiscard]] bool InspectPayloadForAnalysis(spStream&, std::uint32_t,
            InspectionForAnalysis&, std::string* error = nullptr) const;
    private:
        bool ReadTextFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spRenderable&,InspectionForAnalysis*,std::string*) const;
    };
}
