#include "spBoxBVSerializer.h"
#include "spBoxBV.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spBoundingVolumeSize.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateBoxBVSerializer()
        {
            return std::make_unique<spBoxBVSerializer>();
        }

        const spRTTIRecord BoxBVSerializerRecord{
            spBoxBVSerializer::ClassID,
            spSerializer::ClassID,
            "spBoxBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateBoxBVSerializer,
            nullptr,
        };

        const bool BoxBVSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(BoxBVSerializerRecord);
    }

    bool spBoxBVSerializer::Vector3::operator==(
        const Vector3& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool spBoxBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedMirrorOffset == other.decodedMirrorOffset
            && alwaysWritten == other.alwaysWritten;
    }

    spBoxBVSerializer::~spBoxBVSerializer() = default;

    const spRTTIRecord& spBoxBVSerializer::StaticRTTI() noexcept
    {
        (void)BoxBVSerializerRegistered;
        return BoxBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spBoxBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spBoxBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spBoxBVSerializer::vfunc_18() const noexcept
    {
        return BoxBVSerializerRecord;
    }

    spClassID spBoxBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spBoxBVSerializer::FieldBinding>
    spBoxBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x28, 0x18, false},
            {Field::Size, 0x34, 0, true},
        };
    }

    bool spBoxBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= PositionSuppressionEpsilon
            && std::fabs(position.y) <= PositionSuppressionEpsilon
            && std::fabs(position.z) <= PositionSuppressionEpsilon;
    }

    std::vector<spBoxBVSerializer::Field>
    spBoxBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Size);
        return plan;
    }

    spBoxBVSerializer::DerivedSizeState
    spBoxBVSerializer::DecodeSizeForAnalysis(const Vector3& fullSize) noexcept
    {
        const auto state=evidence::pc::bounding_volume::DecodeSize({fullSize.x,fullSize.y,fullSize.z});
        return {{state.halfExtents[0],state.halfExtents[1],state.halfExtents[2]},state.boundingSphereRadius};
    }

    bool spBoxBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Size);
    }
    bool spBoxBVSerializer::ReadScalarFieldForAnalysis(std::uint32_t field,
        spStream& stream,spBoxBV& object)
    {
        if(!IsKnownReadFieldForAnalysis(field))return false;
        spBoxBV::Vector3 value{};
        if(!stream.Read(value))return false;
        if(field==static_cast<std::uint32_t>(Field::Position))object.SetPositionForAnalysis(value);
        else object.SetSizeForAnalysis(value);
        return true;
    }
    bool spBoxBVSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
        auto* box=dynamic_cast<spBoxBV*>(&object);
        if(!box)return cursor.Fail("BoxBV target mismatch");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(IsKnownReadFieldForAnalysis(header->fieldID))
            {
                // Exact field extent belongs to the bounded host envelope.
                if(header->payloadSize!=12||!ReadScalarFieldForAnalysis(header->fieldID,stream,*box))
                    return cursor.Fail("Invalid BoxBV scalar field");
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip BoxBV field");
        }
        return false;
    }
    bool spBoxBVSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,
        spBaseObject& object) const
    {return object.IsExactly(TargetClassID);}
    bool spBoxBVSerializer::WritePayloadForAnalysis(spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* text){if(error)*error=text;return false;};
        const auto* box=dynamic_cast<const spBoxBV*>(&object);
        if(!box)return fail("BoxBV write target mismatch");
        const auto& position=box->GetPositionForAnalysis();const auto& size=box->GetSizeForAnalysis();
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&object))return fail("Cannot begin BoxBV section");
        for(const auto field:BuildWritePlanForAnalysis({{position[0],position[1],position[2]},{size[0],size[1],size[2]}}))
        {
            const auto& value=field==Field::Position?position:size;
            if(!blocks.WriteFieldForAnalysis(stream,static_cast<std::uint32_t>(field),value.data(),sizeof(value)))
                return fail("Cannot write BoxBV field");
        }
        return blocks.FinalizeObjectForAnalysis();
    }
}
