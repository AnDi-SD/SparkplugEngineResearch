#include "spSphereBVSerializer.h"
#include "spSphereBV.h"
#include "Analysis/PC/spSectionCursor.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSphereBVSerializer()
        {
            return std::make_unique<spSphereBVSerializer>();
        }

        const spRTTIRecord SphereBVSerializerRecord{
            spSphereBVSerializer::ClassID,
            spSerializer::ClassID,
            "spSphereBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateSphereBVSerializer,
            nullptr,
        };

        const bool SphereBVSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SphereBVSerializerRecord);
    }

    bool spSphereBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedMirrorOffset == other.decodedMirrorOffset
            && alwaysWritten == other.alwaysWritten;
    }

    spSphereBVSerializer::~spSphereBVSerializer() = default;

    const spRTTIRecord& spSphereBVSerializer::StaticRTTI() noexcept
    {
        (void)SphereBVSerializerRegistered;
        return SphereBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSphereBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSphereBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spSphereBVSerializer::vfunc_18() const noexcept
    {
        return SphereBVSerializerRecord;
    }

    spClassID spSphereBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spSphereBVSerializer::FieldBinding>
    spSphereBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x28, 0x18, false},
            {Field::Radius, 0x34, 0x24, true},
        };
    }

    bool spSphereBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= PositionSuppressionEpsilon
            && std::fabs(position.y) <= PositionSuppressionEpsilon
            && std::fabs(position.z) <= PositionSuppressionEpsilon;
    }

    std::vector<spSphereBVSerializer::Field>
    spSphereBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Radius);
        return plan;
    }

    bool spSphereBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Radius);
    }
    bool spSphereBVSerializer::ReadScalarFieldForAnalysis(std::uint32_t field,
        spStream& stream,spSphereBV& object)
    {
        if(field==static_cast<std::uint32_t>(Field::Position))
        {
            spSphereBV::Vector3 value{};
            if(!stream.Read(value))return false;
            object.SetPositionForAnalysis(value);return true;
        }
        if(field==static_cast<std::uint32_t>(Field::Radius))
        {
            float value=0;
            if(!stream.Read(value))return false;
            object.SetRadiusForAnalysis(value);return true;
        }
        return false;
    }
    bool spSphereBVSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
        auto* sphere=dynamic_cast<spSphereBV*>(&object);
        if(!sphere)return cursor.Fail("SphereBV target mismatch");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(IsKnownReadFieldForAnalysis(header->fieldID))
            {
                // Exact field extent belongs to the bounded host envelope.
                const std::uint32_t expected=header->fieldID==0?12:4;
                if(header->payloadSize!=expected||!ReadScalarFieldForAnalysis(header->fieldID,stream,*sphere))
                    return cursor.Fail("Invalid SphereBV scalar field");
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip SphereBV field");
        }
        return false;
    }
    bool spSphereBVSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,
        spBaseObject& object) const
    {return object.IsExactly(TargetClassID);}
    bool spSphereBVSerializer::WritePayloadForAnalysis(spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* text){if(error)*error=text;return false;};
        const auto* sphere=dynamic_cast<const spSphereBV*>(&object);
        if(!sphere)return fail("SphereBV write target mismatch");
        const auto& position=sphere->GetPositionForAnalysis();
        const float radius=sphere->GetRadiusForAnalysis();
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&object))return fail("Cannot begin SphereBV section");
        for(const auto field:BuildWritePlanForAnalysis({{position[0],position[1],position[2]},radius}))
        {
            const bool written=field==Field::Position
                ?blocks.WriteFieldForAnalysis(stream,0,position.data(),sizeof(position))
                :blocks.WriteFieldForAnalysis(stream,1,&radius,sizeof(radius));
            if(!written)return fail("Cannot write SphereBV field");
        }
        return blocks.FinalizeObjectForAnalysis();
    }
}
