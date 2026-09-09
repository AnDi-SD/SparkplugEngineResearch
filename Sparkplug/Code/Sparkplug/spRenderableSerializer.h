#pragma once

// Inferred declaration/translation-unit path. The class and diagnostics
// survive in both executables, but no original source path was recovered.

#include "spSerializer.h"
#include "Analysis/PC/spReferenceInspection.h"

#include <cstdint>
#include <vector>
#include <optional>

namespace sparkplug::reconstruction
{
    class spRenderable;

    class spRenderableSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x4D694D82;
        static constexpr spClassID TargetClassID = 0x4FDA4542;

        enum class Field : std::uint32_t
        {
            Material = 0,
            Fog = 1,
            AlphaSortEnable = 2,
            AlphaSortPriority = 3,
        };

        spRenderableSerializer() noexcept = default;
        ~spRenderableSerializer() override;

        spRenderableSerializer(const spRenderableSerializer&) = delete;
        spRenderableSerializer& operator=(const spRenderableSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        struct InspectedScalarFieldForAnalysis
        {
            Field field = Field::AlphaSortEnable;
            // Relative to this Renderable section's start, not the physical
            // stream origin or a later Model/Skin section.
            std::uint32_t payloadOffset = 0;
            std::uint32_t payloadSize = 0;
            std::uint32_t assignmentOrder = 0;
            const spRenderable* owner = nullptr;
        };
        // Partial scalar state and unresolved metadata, never a loaded graph.
        struct InspectionForAnalysis
        {
            std::uint32_t fieldMask=0;
            std::optional<evidence::pc::serialization::InspectedReference> material,fog;
            // Host observations from successful actual field2/3 assignments.
            // Repeats retain encounter order; owner borrows the partial object.
            // Consume only after successful whole inspection and while that
            // object is alive. SectionCursor bounds the observation count.
            std::vector<InspectedScalarFieldForAnalysis> scalarFields;
        };

        // Shared slices of original PC secondary47F7A0 -> target1402630.
        // Each writes one complete UInt32 field, including false/zero, without
        // a section terminator or relationship serialization.
        [[nodiscard]] static bool WriteAlphaSortEnableFieldForAnalysis(spStream& destination,
            const spRenderable& object,std::string* error = nullptr);
        [[nodiscard]] static bool WritePriorityFieldForAnalysis(spStream& destination,
            const spRenderable& object,std::string* error = nullptr);

        // Native writer omits null relationships but always emits the two
        // alpha-sort scalars, including their constructor-default values.
        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            const spRenderable& renderable) const;
    protected:
        bool ReadRenderableFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spRenderable&,bool,std::string*,InspectionForAnalysis* = nullptr) const;
        bool WriteRenderableFieldsForAnalysis(spSerializerManager*,spStream&,const spRenderable&,std::string*) const;
        bool IndexRenderableFieldsForAnalysis(spSerializerManager&,spRenderable&) const;
    };
}
