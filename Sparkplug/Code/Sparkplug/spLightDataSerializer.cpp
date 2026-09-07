#include "spLightDataSerializer.h"

#include "spLightData.h"
#include "../SparkplugDX/spDXLight.h"
#include "Analysis/PC/spLightSerializerCodec.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spColorMath.h"
#include <cstring>
#include <limits>

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightDataSerializer()
        {
            return std::make_unique<spLightDataSerializer>();
        }

        const spRTTIRecord LightDataSerializerRecord{
            spLightDataSerializer::ClassID,
            spSerializer::ClassID,
            "spLightDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateLightDataSerializer,
            nullptr,
        };

        const bool LightDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(LightDataSerializerRecord);

        bool IsDefaultColor(const spLight::ColorRGBA& color) noexcept
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

    bool spLightDataSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return nodeFields == other.nodeFields
            && lightFields == other.lightFields;
    }

    spLightDataSerializer::~spLightDataSerializer() = default;

    const spRTTIRecord& spLightDataSerializer::StaticRTTI() noexcept
    {
        (void)LightDataSerializerRegistered;
        return LightDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spLightDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spLightDataSerializer::vfunc_18() const noexcept
    {
        return LightDataSerializerRecord;
    }

    spClassID spLightDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spLightData::ClassID;
    }

    spLightDataSerializer::KnownWritePlan
    spLightDataSerializer::BuildKnownWritePlanForAnalysis(
        const spLightData& light) const
    {
        KnownWritePlan plan;
        plan.nodeFields =
            spNodeSerializer::BuildKnownWritePlanForAnalysis(light);

        for(auto field:evidence::pc::serialization::LightWriteFields(light,defaultWhiteARGBForAnalysis))
            plan.lightFields.push_back(static_cast<Field>(field));

        return plan;
    }
    bool spLightDataSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
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
    bool spLightDataSerializer::WriteSectionsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        const auto* light=dynamic_cast<const spLight*>(&object);
        if(!light){if(error)*error="Light writer target mismatch";return false;}
        return WriteNodeFieldsForAnalysis(manager,stream,*light,error)
            &&evidence::pc::serialization::WriteLightFields(stream,*light,defaultWhiteARGBForAnalysis,error);
    }
    bool spLightDataSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {return WriteSectionsForAnalysis(nullptr,stream,object,error);}
    bool spLightDataSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,
        const spBaseObject& object,std::string* error) const
    {return WriteSectionsForAnalysis(&manager,stream,object,error);}
    bool spLightDataSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* light=dynamic_cast<spLight*>(&object);if(!light)return false;
        for(std::size_t i=0;i<light->GetChildCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,light->GetChildForAnalysis(i)))return false;
        return true;
    }

    std::unique_ptr<spBaseObject> spLightDataSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& stream,spSerializerObjectHeaderForAnalysis* observedHeader) const
    {
        // PC4400B0 ignores both words and unconditionally creates DXLight.
        spSerializerObjectHeaderForAnalysis header;
        if(!stream.ReadData(&header,sizeof(header)))return nullptr;
        if(observedHeader)*observedHeader=header;
        try{return std::make_unique<spDXLight>();}catch(...){return nullptr;}
    }

}
