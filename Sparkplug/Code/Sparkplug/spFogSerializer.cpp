#include "spFogSerializer.h"
#include "spFog.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFogSerializer()
        {
            return std::make_unique<spFogSerializer>();
        }

        const spRTTIRecord FogSerializerRecord{
            spFogSerializer::ClassID,
            spSerializer::ClassID,
            "spFogSerializer",
            &spSerializer::StaticRTTI(),
            &CreateFogSerializer,
            nullptr,
        };

        const bool FogSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(FogSerializerRecord);
    }

    bool spFogSerializer::FogPayload::operator==(
        const FogPayload& other) const noexcept
    {
        return type == other.type
            && colorARGB == other.colorARGB
            && start == other.start
            && end == other.end
            && density == other.density;
    }

    spFogSerializer::~spFogSerializer() = default;

    const spRTTIRecord& spFogSerializer::StaticRTTI() noexcept
    {
        (void)FogSerializerRegistered;
        return FogSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spFogSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spFogSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spFogSerializer::vfunc_18() const noexcept
    {
        return FogSerializerRecord;
    }

    spClassID spFogSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    bool spFogSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* fog=dynamic_cast<spFog*>(&object);
        if(!fog||!object.IsExactly(spFog::ClassID))
        {context.failed=true;if(error)*error="Fog payload target mismatch";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip Fog field");continue;}
            FogPayload payload;static_assert(sizeof(payload)==20);
            // Native mutates after each word. The strict host envelope check
            // rejects truncated fields before any mutation; no rollback claim.
            // Raw type/IEEE float bits are preserved, not clamped or normalized.
            if(!cursor.Read(payload))return cursor.Fail("Fog field requires exactly five32-bit words");
            fog->SetTypeForAnalysis(static_cast<spFog::Type>(payload.type));fog->SetColorARGBForAnalysis(payload.colorARGB);
            fog->SetStartForAnalysis(payload.start);fog->SetEndForAnalysis(payload.end);fog->SetDensityForAnalysis(payload.density);
        }
        return false;
    }

    bool spFogSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* fog=dynamic_cast<const spFog*>(&object);
        if(!fog||!object.IsExactly(spFog::ClassID)){if(error)*error="Fog writer target mismatch";return false;}
        const FogPayload payload{static_cast<std::uint32_t>(fog->GetTypeForAnalysis()),fog->GetColorARGBForAnalysis(),
            fog->GetStartForAnalysis(),fog->GetEndForAnalysis(),fog->GetDensityForAnalysis()};
        spDataBlockSerializer blocks;
        if(blocks.BeginObjectForAnalysis(stream,fog)&&blocks.WriteFieldForAnalysis(stream,0,&payload,sizeof(payload))
            &&blocks.FinalizeObjectForAnalysis())return true;
        if(error)*error="Cannot write Fog section";return false;
    }

    bool spFogSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return object.IsExactly(spFog::ClassID)&&dynamic_cast<spFog*>(&object)!=nullptr;}

    std::vector<spFogSerializer::Field>
    spFogSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::Fog};
    }

    bool spFogSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::Fog);
    }
}
