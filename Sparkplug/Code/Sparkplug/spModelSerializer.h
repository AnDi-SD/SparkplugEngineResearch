#pragma once

// The PS2 executable preserves the original file name
// "spModelSerializer.cpp". No containing source-tree path or header survives.

#include "spRenderableSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spModel;

    class spModelSerializer : public spRenderableSerializer
    {
    public:
        static constexpr spClassID ClassID = 0xDB55C34A;
        static constexpr spClassID TargetClassID = 0x763277DB;
        static constexpr spClassID MeshRelationshipClassID = 0x3F077B6C;

        enum class Field : std::uint32_t
        {
            Base = 0,
            ProjectionGroup = 1,
        };

        struct KnownWritePlan final
        {
            std::vector<spRenderableSerializer::Field> renderableFields;
            std::vector<Field> modelFields;

            [[nodiscard]] bool operator==(const KnownWritePlan& other) const noexcept;
        };

        spModelSerializer() noexcept = default;
        ~spModelSerializer() override;

        spModelSerializer(const spModelSerializer&) = delete;
        spModelSerializer& operator=(const spModelSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        struct InspectionForAnalysis
        {
            spRenderableSerializer::InspectionForAnalysis renderable;
            std::uint32_t fieldMask=0;
            std::optional<evidence::pc::serialization::InspectedReference> mesh;
        };
        bool InspectPayloadForAnalysis(spStream&,std::uint32_t,spModel&,InspectionForAnalysis&,std::string*) const;

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spModel& model) const;
    protected:
        bool ReadModelFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spModel&,bool requireEnd,std::string*,InspectionForAnalysis* = nullptr) const;
        bool IndexModelFieldsForAnalysis(spSerializerManager&,spModel&) const;
        bool WriteModelFieldsForAnalysis(spSerializerManager*,spStream&,const spModel&,std::string*) const;
    };
}
