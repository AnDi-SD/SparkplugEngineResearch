#include "spPyramidProjectionSerializer.h"
#include "spPyramidProjection.h"
#include "Analysis/PC/spSectionCursor.h"
#include <cstring>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPyramidProjectionSerializer>();}
        const spRTTIRecord record{spPyramidProjectionSerializer::ClassID,spProjectionSerializer::ClassID,
            "spPyramidProjectionSerializer",&spProjectionSerializer::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    spPyramidProjectionSerializer::~spPyramidProjectionSerializer()=default;
    const spRTTIRecord& spPyramidProjectionSerializer::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spPyramidProjectionSerializer::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spPyramidProjectionSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPyramidProjectionSerializer>();manager.RegisterCloneForAnalysis(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spClassID spPyramidProjectionSerializer::GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
    bool spPyramidProjectionSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* target=dynamic_cast<spPyramidProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {context.failed=true;if(error)*error="PyramidProjection reader requires a typed section adapter";return false;}
        std::uint32_t start=0,position=0;
        if(!source.GetCurrentPosition(start)
            ||!ReadProjectionFieldsForAnalysis(context,source,size,*target,false,error)
            ||!source.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing PyramidProjection section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,source,size-(position-start),true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {
                if(header->payloadSize!=25)return cursor.Fail("Invalid PyramidProjection parameter record");
                std::array<std::uint32_t,3> raw{};
                if(!source.ReadData(raw.data(),12))return cursor.Fail("Cannot read PyramidProjection center");
                auto center=target->GetSerializedFieldsForAnalysis();
                std::memcpy(center.centerA0.data(),raw.data(),12);
                if(!source.ReadData(raw.data(),12))return cursor.Fail("Cannot read PyramidProjection size");
                auto sizeFields=target->GetSerializedFieldsForAnalysis();
                std::memcpy(sizeFields.sizeAC.data(),raw.data(),12);
                std::uint8_t flip=0;if(!source.Read(flip))return cursor.Fail("Cannot read PyramidProjection flip byte");
                auto flipFields=target->GetSerializedFieldsForAnalysis();
                flipFields.flip9C=flip;
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip PyramidProjection field");
        }
        return false;
    }
    bool spPyramidProjectionSerializer::WritePyramidFieldsForAnalysis(spSerializerManager* manager,
        spStream& stream,const spPyramidProjection& object,std::string* error) const
    {
        if(!WriteProjectionFieldsForAnalysis(manager,stream,object,error))return false;
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&object)
            ||!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt8))
        {if(error&&error->empty())*error="Cannot begin PyramidProjection parameter section";return false;}
        // Original accessor constness is unknown. Its admitted native/default
        // body returns own storage without mutation. Invoke that one existing
        // virtual binding read-only, preserving its three acquisition points.
        auto& source=const_cast<spPyramidProjection&>(object);
        auto center=source.GetSerializedFieldsForAnalysis();
        if(!stream.WriteData(center.centerA0.data(),12))
        {if(error&&error->empty())*error="Cannot write PyramidProjection center";return false;}
        auto size=source.GetSerializedFieldsForAnalysis();
        if(!stream.WriteData(size.sizeAC.data(),12))
        {if(error&&error->empty())*error="Cannot write PyramidProjection size";return false;}
        auto flip=source.GetSerializedFieldsForAnalysis();
        const std::uint8_t byte=flip.flip9C;
        const bool ok=stream.Write(byte)&&blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis();
        if(!ok&&error&&error->empty())*error="Cannot finish PyramidProjection parameter section";return ok;
    }
    bool spPyramidProjectionSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spPyramidProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {if(error)*error="Derived PyramidProjection writer requires its own section adapter";return false;}
        return WritePyramidFieldsForAnalysis(nullptr,stream,*target,error);
    }
    bool spPyramidProjectionSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spPyramidProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {if(error)*error="Derived PyramidProjection writer requires its own section adapter";return false;}
        return WritePyramidFieldsForAnalysis(&manager,stream,*target,error);
    }
}
