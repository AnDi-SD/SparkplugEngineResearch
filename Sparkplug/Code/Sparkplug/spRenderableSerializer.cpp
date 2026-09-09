#include "spRenderableSerializer.h"

#include "spRenderable.h"
#include "spMaterial.h"
#include "spFog.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderableSerializer()
        {
            return std::make_unique<spRenderableSerializer>();
        }

        const spRTTIRecord RenderableSerializerRecord{
            spRenderableSerializer::ClassID,
            spSerializer::ClassID,
            "spRenderableSerializer",
            &spSerializer::StaticRTTI(),
            &CreateRenderableSerializer,
            nullptr,
        };

        const bool RenderableSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(RenderableSerializerRecord);
    }

    spRenderableSerializer::~spRenderableSerializer() = default;

    const spRTTIRecord& spRenderableSerializer::StaticRTTI() noexcept
    {
        (void)RenderableSerializerRegistered;
        return RenderableSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spRenderableSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderableSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spRenderableSerializer::vfunc_18() const noexcept
    {
        return RenderableSerializerRecord;
    }

    spClassID spRenderableSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spRenderable::ClassID;
    }

    bool spRenderableSerializer::ReadRenderableFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spRenderable& object,bool exact,std::string* error,InspectionForAnalysis* observation) const
    {
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,exact,error);
        std::uint32_t sectionStart=0;
        if(observation&&!stream.GetCurrentPosition(sectionStart))return cursor.Fail("Cannot locate Renderable inspection section");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID<2)
            {
                if(observation)
                {
                    evidence::pc::serialization::InspectedReference reference;
                    if(!evidence::pc::serialization::InspectReference(stream,header->payloadSize,true,reference,error))
                        return cursor.Fail("Cannot inspect Renderable relationship");
                    (header->fieldID==0?observation->material:observation->fog)=reference;
                    observation->fieldMask|=1u<<header->fieldID;continue;
                }
                const auto expected=header->fieldID==0?spMaterial::ClassID:spFog::ClassID;
                auto* raw=ReadFieldReferenceForAnalysis(context,expected,stream,*header,error);
                if(context.failed)return false;
                auto owner=context.ShareObjectForAnalysis(raw);
                if(raw&&(!owner||!raw->IsKindOf(expected)))return cursor.Fail("Renderable relationship type/owner mismatch");
                if(header->fieldID==0)object.SetMaterialForAnalysis(std::move(owner));else object.SetFogForAnalysis(std::move(owner));
            }
            else if(header->fieldID==2||header->fieldID==3)
            {
                std::uint32_t value=0;if(!cursor.Read(value))return cursor.Fail("Invalid Renderable UInt32 scalar");
                if(header->fieldID==2)object.SetAlphaSortEnabledForAnalysis(value!=0);else object.SetPriorityForAnalysis(value);
                if(observation)
                {
                    observation->fieldMask|=1u<<header->fieldID;
                    observation->scalarFields.push_back({static_cast<Field>(header->fieldID),
                        header->dataStreamPosition-sectionStart,header->payloadSize,
                        static_cast<std::uint32_t>(observation->scalarFields.size()),&object});
                }
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip Renderable field");
        }
        return false;
    }

    bool spRenderableSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* target=dynamic_cast<spRenderable*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=spRenderable::ClassID)
        {context.failed=true;if(error)*error="Derived Renderable serializer requires own section adapter";return false;}
        return ReadRenderableFieldsForAnalysis(context,stream,size,*target,true,error);
    }

    bool spRenderableSerializer::IndexRenderableFieldsForAnalysis(spSerializerManager& manager,spRenderable& object) const
    {
        return IndexReferenceForAnalysis(manager,object.GetMaterialForAnalysis().get())
            &&IndexReferenceForAnalysis(manager,object.GetFogForAnalysis().get());
    }

    bool spRenderableSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* target=dynamic_cast<spRenderable*>(&object);
        return target&&GetTargetClassIDForAnalysis()==spRenderable::ClassID&&IndexRenderableFieldsForAnalysis(manager,*target);
    }

    bool spRenderableSerializer::WriteAlphaSortEnableFieldForAnalysis(spStream& stream,
        const spRenderable& object,std::string* error)
    {
        if(error)error->clear();
        const std::uint32_t value=std::uint32_t(object.IsAlphaSortEnabledForAnalysis());
        if(!spDataBlockSerializer{}.WriteFieldForAnalysis(stream,2,&value,sizeof(value)))
        {if(error)*error="Cannot write Renderable alpha-sort enable";return false;}
        return true;
    }

    bool spRenderableSerializer::WritePriorityFieldForAnalysis(spStream& stream,
        const spRenderable& object,std::string* error)
    {
        if(error)error->clear();
        const auto value=object.GetPriorityForAnalysis();
        if(!spDataBlockSerializer{}.WriteFieldForAnalysis(stream,3,&value,sizeof(value)))
        {if(error)*error="Cannot write Renderable alpha-sort priority";return false;}
        return true;
    }

    bool spRenderableSerializer::WriteRenderableFieldsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spRenderable& object,std::string* error) const
    {
        if(error)error->clear();
        if(!manager&&(object.GetMaterialForAnalysis()||object.GetFogForAnalysis()))
        {if(error)*error="Renderable graph writer requires explicit manager";return false;}
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&object))return false;
        for(auto field:BuildKnownWritePlanForAnalysis(object))
        {
            const auto id=static_cast<std::uint32_t>(field);
            if(id<2)
            {
                auto* target=(id==0?object.GetMaterialForAnalysis():object.GetFogForAnalysis()).get();
                if(!target->IsKindOf(id==0?spMaterial::ClassID:spFog::ClassID))
                {if(error)*error="Renderable write relationship type mismatch";return false;}
                if(!blocks.WriteBeginForAnalysis(id,spDataBlockSerializer::SizeCode::UInt32)
                    ||!WriteReferenceForAnalysis(*manager,stream,target,error)||!blocks.WriteEndForAnalysis(id))return false;
            }
            else if(id==2)
            {if(!WriteAlphaSortEnableFieldForAnalysis(stream,object,error))return false;}
            else if(!WritePriorityFieldForAnalysis(stream,object,error))return false;
        }
        return blocks.FinalizeObjectForAnalysis();
    }

    bool spRenderableSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* target=dynamic_cast<const spRenderable*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=spRenderable::ClassID)
        {if(error)*error="Derived Renderable writer requires own section adapter";return false;}
        return WriteRenderableFieldsForAnalysis(nullptr,stream,*target,error);
    }

    bool spRenderableSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* target=dynamic_cast<const spRenderable*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=spRenderable::ClassID)
        {if(error)*error="Derived Renderable writer requires own section adapter";return false;}
        return WriteRenderableFieldsForAnalysis(&manager,stream,*target,error);
    }

    std::vector<spRenderableSerializer::Field>
    spRenderableSerializer::BuildKnownWritePlanForAnalysis(
        const spRenderable& renderable) const
    {
        std::vector<Field> plan;
        if (renderable.GetMaterialForAnalysis())
        {
            plan.push_back(Field::Material);
        }
        if (renderable.GetFogForAnalysis())
        {
            plan.push_back(Field::Fog);
        }
        plan.push_back(Field::AlphaSortEnable);
        plan.push_back(Field::AlphaSortPriority);
        return plan;
    }
}
