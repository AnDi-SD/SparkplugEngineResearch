#pragma once

// The class and serializer diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"
#include "Analysis/PC/spReferenceInspection.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spAnimTexController;
    class spAnimTexControllerSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x77793754;
        static constexpr spClassID TargetClassID = 0x16FB0E47;
        static constexpr spClassID TextureRelationshipClassID = 0x2F281E13;

        enum class Field : std::uint32_t
        {
            ControllerBase = 0,
        };

        enum class PayloadSegment : std::uint32_t
        {
            FrameCount,
            TimeArray,
            TextureRelationships,
        };

        struct SegmentPlan final
        {
            PayloadSegment segment;
            std::uint32_t elementCount;
            std::uint64_t fixedByteCount;
            bool variableLength;

            [[nodiscard]] bool operator==(const SegmentPlan& other) const noexcept;
        };

        using PayloadPlan = std::array<SegmentPlan, 3>;

        spAnimTexControllerSerializer() noexcept;
        ~spAnimTexControllerSerializer() override;

        spAnimTexControllerSerializer(const spAnimTexControllerSerializer&) = delete;
        spAnimTexControllerSerializer& operator=(
            const spAnimTexControllerSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis();
        [[nodiscard]] static PayloadPlan BuildPayloadPlanForAnalysis(
            std::uint32_t frameCount) noexcept;
        [[nodiscard]] static bool HasConsistentTrackShapeForAnalysis(
            std::uint32_t timeCount,
            std::uint32_t textureRelationshipCount) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const override;
        struct InspectionForAnalysis
        {
            bool hasTrack=false;
            std::vector<evidence::pc::serialization::InspectedReference> textures;
        };
        // Partial track keeps real times and NULL slots; unresolved resources
        // remain metadata. No substitute Texture object is instantiated.
        bool InspectPayloadForAnalysis(spStream&,std::uint32_t,spAnimTexController&,
            InspectionForAnalysis&,std::string*) const;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,
            spStream& destination,const spBaseObject& object,std::string* error) const override;
    private:
        bool ReadFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,
            spBaseObject&,std::string*,InspectionForAnalysis*) const;
    };
}
