#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
//   Z:\Sparkplug\Code\Sparkplug\spSkinSerializer.cpp
// The header path and ForAnalysis names are reconstruction choices.

#include "spModelSerializer.h"
#include "spSkin.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spSkin;

    class spSkinSerializer final : public spModelSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x120D33C7;
        static constexpr spClassID TargetClassID = 0x681F2043;
        static constexpr spClassID BoneRelationshipClassID = 0x695C0F65;

        enum class Field : std::uint32_t
        {
            Skin = 0,
        };

        enum class PayloadSegment
        {
            WeightCount,
            BoneCount,
            BoneRelationship,
            InverseBindMatrix,
        };

        struct PayloadPlanEntry final
        {
            PayloadSegment segment;
            std::size_t elementCount;
            std::size_t byteCount;

            [[nodiscard]] bool operator==(
                const PayloadPlanEntry& other) const noexcept;
        };

        struct KnownWritePlan final
        {
            spModelSerializer::KnownWritePlan model;
            std::vector<Field> skinFields;
            std::vector<PayloadPlanEntry> payload;

            [[nodiscard]] bool operator==(const KnownWritePlan& other) const noexcept;
        };

        spSkinSerializer() noexcept = default;
        ~spSkinSerializer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        struct InspectedBoneForAnalysis
        {
            evidence::pc::serialization::InspectedReference reference;
            spSkin::Matrix4 inverseBind{};
        };
        struct InspectedPaletteFieldForAnalysis
        {
            // Relative to the whole Skin payload input, including inherited
            // Renderable/Model sections but excluding the object header.
            std::uint32_t headerOffset=0,payloadOffset=0,payloadSize=0;
            std::uint32_t assignmentOrder=0;
        };
        struct InspectionForAnalysis
        {
            spModelSerializer::InspectionForAnalysis model;
            std::uint32_t fieldMask=0,weights=0;
            std::vector<InspectedBoneForAnalysis> bones;
            // Host locations from the actual Skin field0 reader only. Repeats
            // retain encounter order; weights/bones above retain the last
            // complete assignment. Consume rows only after whole inspection
            // succeeds. The existing SectionCursor field cap bounds storage.
            std::vector<InspectedPaletteFieldForAnalysis> paletteFields;
        };
        // Unresolved bone IDs/matrices remain metadata. No substitute Node
        // palette is attached to the explicitly partial Skin object.
        bool InspectPayloadForAnalysis(spStream&,std::uint32_t,spSkin&,InspectionForAnalysis&,std::string*) const;

        // Shared field body of PC490DA0: UInt32-length field0, weight/count,
        // then actual reference writer and raw64-byte matrix for each bone.
        // No section terminator. A null manager preserves the existing empty
        // palette path only; nonempty bindings require their real owner/index.
        [[nodiscard]] static bool WritePaletteFieldWithContextForAnalysis(spSerializerManager*,
            spStream&,const spSkin&,std::string* = nullptr);
        [[nodiscard]] static bool WritePaletteFieldWithContextForAnalysis(spSerializerManager& manager,
            spStream& stream,const spSkin& skin,std::string* error = nullptr)
        {return WritePaletteFieldWithContextForAnalysis(&manager,stream,skin,error);}

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spSkin& skin) const;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    private:
        bool ReadSectionsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,
            spSkin&,std::string*,InspectionForAnalysis*) const;
        bool WriteSectionsForAnalysis(spSerializerManager*,spStream&,const spSkin&,std::string*) const;
    };
}
