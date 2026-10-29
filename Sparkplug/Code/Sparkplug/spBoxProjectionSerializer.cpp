#include "spBoxProjectionSerializer.h"
#include "spBoxProjection.h"
#include "Analysis/PC/spSectionCursor.h"
#include <cstring>
#include <utility>
#include <typeinfo>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spBoxProjectionSerializer>();}
        const spRTTIRecord record{spBoxProjectionSerializer::ClassID,spProjectionSerializer::ClassID,
            "spBoxProjectionSerializer",&spProjectionSerializer::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    spBoxProjectionSerializer::~spBoxProjectionSerializer()=default;
    const spRTTIRecord& spBoxProjectionSerializer::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spBoxProjectionSerializer::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spBoxProjectionSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spBoxProjectionSerializer>();manager.RegisterCloneForAnalysis(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spClassID spBoxProjectionSerializer::GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
    bool spBoxProjectionSerializer::IndexNullRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionNullIndexContextForAnalysis& context,spBaseObject& object) const
    {
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&GetTargetClassIDForAnalysis()==TargetClassID
            &&IndexProjectionNullFieldsForAnalysis(context,*target);
    }
    bool spBoxProjectionSerializer::IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionPreindexedIndexContextForAnalysis& context,spBaseObject& object) const
    {
        if(typeid(*this)!=typeid(spBoxProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionPreindexedFieldsForAnalysis(context,*target);
    }
    bool spBoxProjectionSerializer::IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionFogOnlyIndexContextForAnalysis& context,spBaseObject& object) const
    {
        if(typeid(*this)!=typeid(spBoxProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionFogOnlyFieldsForAnalysis(context,*target);
    }
    bool spBoxProjectionSerializer::IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionEmptyMaterialIndexContextForAnalysis& context,spBaseObject& object) const
    {
        if(typeid(*this)!=typeid(spBoxProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionEmptyMaterialFieldsForAnalysis(context,*target);
    }
    bool spBoxProjectionSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* target=dynamic_cast<spBoxProjection*>(&object);
        if(!target){context.failed=true;if(error)*error="BoxProjection reader requires a typed BoxProjection";return false;}
        std::uint32_t start=0,position=0;
        if(!source.GetCurrentPosition(start)
            ||!ReadProjectionFieldsForAnalysis(context,source,size,*target,false,error)
            ||!source.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing BoxProjection section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,source,size-(position-start),true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {
                if(header->payloadSize!=24)return cursor.Fail("Invalid BoxProjection vector pair");
                std::array<std::uint32_t,3> raw{};
                if(!source.ReadData(raw.data(),12))return cursor.Fail("Cannot read BoxProjection center");
                // Native commits the complete first vector before reading the
                // second. Preserve all raw float bits without evaluating them.
                std::memcpy(target->center9C_.data(),raw.data(),12);
                if(!source.ReadData(raw.data(),12))return cursor.Fail("Cannot read BoxProjection size");
                std::memcpy(target->sizeA8_.data(),raw.data(),12);
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip BoxProjection field");
        }
        return false;
    }
    bool spBoxProjectionSerializer::WriteBoxFieldsForAnalysis(spSerializerManager* manager,
        spStream& stream,const spBoxProjection& object,std::string* error) const
    {
        if(!WriteProjectionFieldsForAnalysis(manager,stream,object,error))return false;
        spDataBlockSerializer blocks;
        const bool ok=blocks.BeginObjectForAnalysis(stream,&object)
            &&blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt8)
            &&stream.WriteData(object.GetCenterForAnalysis().data(),12)
            &&stream.WriteData(object.GetSizeForAnalysis().data(),12)
            &&blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis();
        if(!ok&&error&&error->empty())*error="Cannot write BoxProjection geometry section";return ok;
    }
    bool spBoxProjectionSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spBoxProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {if(error)*error="Derived BoxProjection writer requires its own section adapter";return false;}
        return WriteBoxFieldsForAnalysis(nullptr,stream,*target,error);
    }
    bool spBoxProjectionSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spBoxProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {if(error)*error="Derived BoxProjection writer requires its own section adapter";return false;}
        return WriteBoxFieldsForAnalysis(&manager,stream,*target,error);
    }
}
