#include "spLightSerializer.h"

#include "spLight.h"
#include "Analysis/PC/spLightSerializerCodec.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spColorMath.h"
#include <cstring>
#include <limits>

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::evidence::pc::serialization
{
    using namespace reconstruction;
    namespace
    {
        std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    }
    std::vector<std::uint32_t> LightWriteFields(const spLight& light,std::uint32_t defaultWhiteARGB)
    {
        std::vector<std::uint32_t> fields;
        if(static_cast<std::uint32_t>(light.GetTypeForAnalysis()))fields.push_back(0);
        if(light.ProjectsShadowVolumeForAnalysis())fields.push_back(1);
        const auto white=PCARGBToRGBAForAnalysis(defaultWhiteARGB);const auto& color=light.GetColorForAnalysis();
        for(std::size_t i=0;i<4;++i)if(!(std::fabs(double(color[i])-double(white[i]))<=double(spNodeSerializer::DefaultComparisonTolerance)))
        {fields.push_back(2);break;}
        if(light.UsesAttenuationForAnalysis())fields.push_back(3);
        // The original compares these two raw words, angles use x87 comparisons.
        if(Bits(light.GetIntensityForAnalysis())!=0x3f800000u)fields.push_back(4);
        if(Bits(light.GetRangeForAnalysis())!=0x43480000u)fields.push_back(5);
        if(light.GetHotspotAngleForAnalysis()!=0.f)fields.push_back(6);
        if(light.GetFalloffAngleForAnalysis()!=0.f)fields.push_back(7);
        if(light.IsLightEnabledForAnalysis())fields.push_back(8);
        return fields;
    }
    bool ReadLightFields(spSerializerReadContextForAnalysis& context,spStream& stream,
        std::uint32_t size,spLight& light,std::string* error)
    {
        SectionCursor cursor(context,stream,size,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            const auto field=header->fieldID;
            if(field>8){if(!cursor.Skip())return cursor.Fail("Cannot skip light field");continue;}
            if(field==1||field==3||field==8)
            {
                std::uint8_t value=0;if(!cursor.Read(value))return cursor.Fail("Light flag needs one byte");
                // Host bool representation canonicalizes any nonzero native byte.
                if(field==1)light.SetProjectsShadowVolumeForAnalysis(value!=0);
                else if(field==3)light.SetUsesAttenuationForAnalysis(value!=0);
                else light.SetLightEnabledForAnalysis(value!=0);
            }
            else
            {
                std::uint32_t value=0;if(!cursor.Read(value))return cursor.Fail("Light scalar needs four bytes");
                float scalar;std::memcpy(&scalar,&value,4);
                switch(field)
                {
                case 0:light.SetTypeForAnalysis(static_cast<spLight::Type>(value));break;
                case 2:light.SetColorForAnalysis(PCARGBToRGBAForAnalysis(value));break;
                case 4:light.SetIntensityForAnalysis(scalar);break;
                case 5:light.SetRangeForAnalysis(scalar);break;
                case 6:light.SetHotspotAngleForAnalysis(scalar);break;
                case 7:light.SetFalloffAngleForAnalysis(scalar);break;
                }
            }
            light.MarkLightDataDirtyForAnalysis();
        }
        return false;
    }
    bool WriteLightFields(spStream& stream,const spLight& light,std::uint32_t white,std::string* error)
    {
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&light))return fail("Cannot begin light section");
        for(auto field:LightWriteFields(light,white))
        {
            const auto write=[&](const auto& value){return blocks.WriteFieldForAnalysis(stream,field,&value,sizeof(value));};
            bool ok=false;
            switch(field)
            {
            case 0:ok=write(static_cast<std::uint32_t>(light.GetTypeForAnalysis()));break;
            case 1:ok=write(std::uint8_t(light.ProjectsShadowVolumeForAnalysis()));break;
            case 2:
            {
                std::uint32_t color=0;const unsigned shifts[]={16,8,0,24};std::size_t i=0;
                for(float component:light.GetColorForAnalysis())
                {
                    const double value=double(component)*255.;
                    if(!std::isfinite(value)||value<double(std::numeric_limits<std::int32_t>::min())
                        ||value>double(std::numeric_limits<std::int32_t>::max()))return fail("Light color exceeds bounded native integer conversion");
                    color|=(static_cast<std::uint32_t>(static_cast<std::int32_t>(value))&255u)<<shifts[i++];
                }
                ok=write(color);break;
            }
            case 3:ok=write(std::uint8_t(light.UsesAttenuationForAnalysis()));break;
            case 4:ok=write(Bits(light.GetIntensityForAnalysis()));break;
            case 5:ok=write(Bits(light.GetRangeForAnalysis()));break;
            case 6:ok=write(Bits(light.GetHotspotAngleForAnalysis()));break;
            case 7:ok=write(Bits(light.GetFalloffAngleForAnalysis()));break;
            case 8:ok=write(std::uint8_t(light.IsLightEnabledForAnalysis()));break;
            }
            if(!ok)return fail("Cannot write light field");
        }
        return blocks.FinalizeObjectForAnalysis()?true:fail("Cannot finish light section");
    }
}

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightSerializer()
        {
            return std::make_unique<spLightSerializer>();
        }

        const spRTTIRecord LightSerializerRecord{
            spLightSerializer::ClassID,
            spNodeSerializer::ClassID,
            "spLightSerializer",
            &spNodeSerializer::StaticRTTI(),
            &CreateLightSerializer,
            nullptr,
        };

        const bool LightSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(LightSerializerRecord);

        bool IsDefaultLightColor(const spLight::ColorRGBA& color) noexcept
        {
            for (const float component : color)
            {
                if (std::fabs(component - 1.0F)
                    > spNodeSerializer::DefaultComparisonTolerance)
                {
                    return false;
                }
            }
            return true;
        }
    }

    bool spLightSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return nodeFields == other.nodeFields
            && lightFields == other.lightFields;
    }

    spLightSerializer::~spLightSerializer() = default;

    const spRTTIRecord& spLightSerializer::StaticRTTI() noexcept
    {
        (void)LightSerializerRegistered;
        return LightSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spLightSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spLightSerializer::vfunc_18() const noexcept
    {
        return LightSerializerRecord;
    }

    spClassID spLightSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spLight::ClassID;
    }

    spLightSerializer::KnownWritePlan
    spLightSerializer::BuildKnownWritePlanForAnalysis(const spLight& light) const
    {
        KnownWritePlan plan;
        plan.nodeFields =
            spNodeSerializer::BuildKnownWritePlanForAnalysis(light);

        for(auto field:evidence::pc::serialization::LightWriteFields(light,defaultWhiteARGBForAnalysis))
            plan.lightFields.push_back(static_cast<Field>(field));

        return plan;
    }
    bool spLightSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* light=dynamic_cast<spLight*>(&object);
        if(!light){context.failed=true;if(error)*error="Light payload target mismatch";return false;}
        std::uint32_t start=0,position=0;
        if(!stream.GetCurrentPosition(start)||!ReadNodeFieldsForAnalysis(context,stream,size,*light,false,error)
            ||!stream.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing light section";return false;}
        return evidence::pc::serialization::ReadLightFields(context,stream,size-(position-start),*light,error);
    }
    bool spLightSerializer::WriteSectionsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        const auto* light=dynamic_cast<const spLight*>(&object);
        if(!light){if(error)*error="Light writer target mismatch";return false;}
        return WriteNodeFieldsForAnalysis(manager,stream,*light,error)
            &&evidence::pc::serialization::WriteLightFields(stream,*light,defaultWhiteARGBForAnalysis,error);
    }
    bool spLightSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {return WriteSectionsForAnalysis(nullptr,stream,object,error);}
    bool spLightSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,
        const spBaseObject& object,std::string* error) const
    {return WriteSectionsForAnalysis(&manager,stream,object,error);}
    bool spLightSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* light=dynamic_cast<spLight*>(&object);if(!light)return false;
        for(std::size_t i=0;i<light->GetChildCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,light->GetChildForAnalysis(i)))return false;
        return true;
    }

}
