#include "spProjectionSerializer.h"
#include "spProjection.h"
#include "spRenderNode.h"
#include "spFog.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "spResourceFATSerializer.h"
#include "spSerializerManager.h"
#include "Analysis/PC/spSectionCursor.h"
#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spProjectionSerializer>();}
        const spRTTIRecord record{spProjectionSerializer::ClassID,spRenderableSerializer::ClassID,
            "spProjectionSerializer",&spRenderableSerializer::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool FailWrite(std::string* error,const char* message)
        {if(error&&error->empty())*error=message;return false;}
        bool AdmitWrite(const spProjection& object,spSerializerManager* manager,std::string* error)
        {
            auto* fat=manager?manager->GetFATForAnalysis():nullptr;
            const auto* material=object.GetMaterialForAnalysis().get();
            const auto* fog=object.GetFogForAnalysis().get();
            if((material&&!dynamic_cast<const spDXMaterial*>(material))
                ||(fog&&!dynamic_cast<const spFog*>(fog)))
                return FailWrite(error,"Projection writer requires qualified DXMaterial/Fog types");
            const auto written=[&](const spBaseObject* value)
            {
                if(!value)return true;
                const auto* entry=fat?fat->FindByObjectForAnalysis(*value):nullptr;
                return entry&&entry->payloadWritten;
            };
            return (written(material)&&written(fog)&&written(object.GetBoundRenderNodeForAnalysis()))
                ||FailWrite(error,"Projection writer requires externally retained, already written relationship FAT inputs");
        }
        template<class Payload>bool WriteFixedPair(spStream& stream,std::uint32_t field,Payload&& payload)
        {
            // The native pair reserves Fixed8, writes two words separately,
            // then seeks back and rewrites its one-byte header. The general
            // portable DataBlock builder refuses fixed reservations; spell
            // this exact8-byte profile through its shared header encoder.
            using Blocks=spDataBlockSerializer;std::uint32_t start=0,end=0;
            if(!stream.GetCurrentPosition(start)||start>std::uint32_t(std::numeric_limits<std::int32_t>::max())-9
                ||!Blocks::WriteHeaderWithCodeForAnalysis(stream,field,8,Blocks::SizeCode::Fixed8)||!payload()
                ||!stream.GetCurrentPosition(end)||end-start!=9)return false;
            return stream.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(start))
                &&Blocks::WriteHeaderWithCodeForAnalysis(stream,field,8,Blocks::SizeCode::Fixed8)
                &&stream.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(end));
        }
    }
    spProjectionSerializer::~spProjectionSerializer()=default;
    const spRTTIRecord& spProjectionSerializer::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spProjectionSerializer::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spProjectionSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spProjectionSerializer>();manager.RegisterCloneForAnalysis(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spClassID spProjectionSerializer::GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
    bool spProjectionSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* target=dynamic_cast<spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {context.failed=true;if(error)*error="Projection reader requires its own compatible section adapter";return false;}
        return ReadProjectionFieldsForAnalysis(context,source,size,*target,true,error);
    }
    bool spProjectionSerializer::ReadProjectionFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spProjection& object,bool requireEnd,std::string* error) const
    {
        std::uint32_t start=0,position=0;
        if(!source.GetCurrentPosition(start)
            ||!ReadRenderableFieldsForAnalysis(context,source,size,object,false,error)
            ||!source.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing Projection section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,source,size-(position-start),requireEnd,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID<2)
            {
                if(header->payloadSize!=8)return cursor.Fail("Invalid Projection word pair");
                const auto first=2u*header->fieldID;std::uint32_t word=0;
                // Native commits the first word before reading the second.
                if(!source.Read(word))return cursor.Fail("Cannot read first Projection word");
                object.state_.words5C[first]=word;
                if(!source.Read(word))return cursor.Fail("Cannot read second Projection word");
                object.state_.words5C[first+1]=word;
            }
            else if(header->fieldID<=4)
            {
                std::uint8_t byte=0;if(!cursor.Read(byte))return cursor.Fail("Invalid Projection byte");
                if(header->fieldID==2)object.state_.word6C=byte;
                else object.state_.flags58[header->fieldID-2]=byte;
            }
            else if(header->fieldID==5)
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spRenderNode::ClassID,source,*header,error);
                if(context.failed)return false;
                auto* node=dynamic_cast<spRenderNode*>(raw);
                // Native resolver ignores expectedClassID. Reject a wrong
                // graph explicitly before publishing an unsafe typed pointer.
                if(raw&&!node)return cursor.Fail("Projection bound relationship is not a typed RenderNode");
                object.SetBoundRenderNodeForAnalysis(node);
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip Projection field");
        }
        return false;
    }
    bool spProjectionSerializer::IndexRelationshipsForAnalysis(spBaseObject&) const{return false;}
    bool spProjectionSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const{return false;}
    bool spProjectionSerializer::WriteProjectionFieldsForAnalysis(spSerializerManager* manager,
        spStream& stream,const spProjection& object,std::string* error) const
    {
        if(!AdmitWrite(object,manager,error)||!WriteRenderableFieldsForAnalysis(manager,stream,object,error))return false;
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&object))return FailWrite(error,"Cannot begin Projection write section");
        const auto& state=object.GetProjectionStateForAnalysis();
        for(std::uint32_t field=0;field<2;++field)
        {
            const auto first=2*field;
            if(state.words5C[first]!=0x3F800000||state.words5C[first+1]!=0x3F800000)
                if(!WriteFixedPair(stream,field,[&](){return stream.Write(state.words5C[first])&&stream.Write(state.words5C[first+1]);}))
                    return FailWrite(error,"Cannot write Projection raw word pair");
        }
        const auto word=state.word6C;
        if(word!=2)
        {
            const auto byte=static_cast<std::uint8_t>(word);
            if(!blocks.WriteFieldForAnalysis(stream,2,&byte,1))return FailWrite(error,"Cannot write Projection low byte");
        }
        for(std::uint32_t field=3;field<=4;++field)
        {
            const auto byte=state.flags58[field-2];
            if(byte&&!blocks.WriteFieldForAnalysis(stream,field,&byte,1))return FailWrite(error,"Cannot write Projection raw flag");
        }
        if(const auto* node=object.GetBoundRenderNodeForAnalysis())
        {
            if(!blocks.WriteBeginForAnalysis(5,spDataBlockSerializer::SizeCode::UInt32)
                ||!WriteReferenceForAnalysis(*manager,stream,node,error)||!blocks.WriteEndForAnalysis(5))
                return FailWrite(error,"Cannot write Projection existing Node reference");
        }
        return blocks.FinalizeObjectForAnalysis()||FailWrite(error,"Cannot terminate Projection write section");
    }
    bool spProjectionSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)return FailWrite(error,"Derived Projection writer requires its own section adapter");
        return WriteProjectionFieldsForAnalysis(nullptr,stream,*target,error);
    }
    bool spProjectionSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)return FailWrite(error,"Derived Projection writer requires its own section adapter");
        return WriteProjectionFieldsForAnalysis(&manager,stream,*target,error);
    }
}
