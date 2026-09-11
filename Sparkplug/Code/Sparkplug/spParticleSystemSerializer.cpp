#include "spParticleSystemSerializer.h"
#include "spRenderNode.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spParticleSampling.h"
#include <cmath>
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSerializer(){return std::make_unique<spParticleSystemSerializer>();}
        const spRTTIRecord Record{spParticleSystemSerializer::ClassID,spRenderableSerializer::ClassID,
            "spParticleSystemSerializer",&spRenderableSerializer::StaticRTTI(),&CreateSerializer,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        constexpr std::array<std::uint32_t,7> RegionTypes{1,4,2,3,5,6,7};
        constexpr std::array<std::uint32_t,7> RegionSizes{3,8,6,4,4,5,6};
        bool Different(const spParticleSystem::Vector3& a,const spParticleSystem::Vector3& b)
        {for(unsigned i=0;i<3;++i)if(!(std::abs(double(a[i])-b[i])<=double(0.001f)))return true;return false;}
        bool BitsEqual(float a,float b){return std::memcmp(&a,&b,sizeof(a))==0;}
        bool Finite(const spParticleSystem::ParametersForAnalysis& p)
        {
            const auto finite=[](const auto& values){for(float v:values)if(!std::isfinite(v))return false;return true;};
            return finite(p.acceleration[0])&&finite(p.acceleration[1])&&finite(p.direction)&&finite(p.velocity)
                &&finite(p.angle)&&finite(p.scale)&&finite(p.times)&&finite(p.sphere)&&finite(p.region)&&std::isfinite(p.rate);
        }
    }
    const spRTTIRecord& spParticleSystemSerializer::StaticRTTI() noexcept {(void)Registered;return Record;}
    const spRTTIRecord& spParticleSystemSerializer::vfunc_18() const noexcept {return Record;}
    std::unique_ptr<spBaseObject> spParticleSystemSerializer::vfunc_10(spCloneManager&) const {return nullptr;}
    bool spParticleSystemSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& base,std::string* error) const
    {
        if(error)error->clear();auto* object=dynamic_cast<spParticleSystem*>(&base);
        std::uint32_t start=0,position=0;
        if(!object||!stream.GetCurrentPosition(start)||!ReadRenderableFieldsForAnalysis(context,stream,size,*object,false,error)
            ||!stream.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing ParticleSystem section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,stream,size-(position-start),true,error);
        auto& p=object->Parameters();bool regionSeen=false;
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())
            {
                if(!Finite(p)||!regionSeen)
                    return cursor.Fail("Particle initialization requires finite parameters and one region");
                std::string reason;
                if(!object->InitializeForAnalysis(nullptr,&reason))return cursor.Fail(reason.c_str());
                return true;
            }
            const auto id=header->fieldID;bool read=false;
            if(id==0)read=cursor.Read(p.acceleration);
            else if(id==1)
            {
                read=cursor.Read(p.direction);
                if(read)
                {
                    for(float v:p.direction)if(!std::isfinite(v))return cursor.Fail("Nonfinite particle direction");
                    evidence::pc::NormalizeParticleDirectionForAnalysis(p.direction);
                }
            }
            else if(id==2)read=cursor.Read(p.velocity);
            else if(id==3)read=cursor.Read(p.angle);
            else if(id==4)read=cursor.Read(p.scale);
            else if(id==5)read=cursor.Read(p.colors);
            else if(id==6)read=cursor.Read(p.times);
            else if(id>=7&&id<=9)read=cursor.Read(p.flags[id-7]);
            else if(id==10)read=cursor.Read(p.rate);
            else if(id==11){p.sphere={};read=cursor.Read(p.sphere[3]);}
            else if(id>=12&&id<=18)
            {
                if(regionSeen)return cursor.Fail("Repeated Particle emission region");
                regionSeen=true;p.regionType=RegionTypes[id-12];p.region.resize(RegionSizes[id-12]);
                read=header->payloadSize==p.region.size()*sizeof(float)&&stream.ReadData(p.region.data(),header->payloadSize);
            }
            else if(id==19)
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spRenderNode::ClassID,stream,*header,error);
                if(context.failed)return false;
                auto node=std::dynamic_pointer_cast<spRenderNode>(context.ShareObjectForAnalysis(raw));
                if(!node)return cursor.Fail("Particle render node requires an explicit graph owner");
                object->SetRenderNodeForAnalysis(node);read=true;
            }
            else read=cursor.Skip();
            if(!read)return cursor.Fail("Invalid Particle field extent");
        }
        return false;
    }
    bool spParticleSystemSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& base) const
    {
        auto* object=dynamic_cast<spParticleSystem*>(&base);
        return object&&IndexRenderableFieldsForAnalysis(manager,*object)
            &&IndexReferenceForAnalysis(manager,object->GetRenderNodeForAnalysis().get());
    }
    bool spParticleSystemSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& base,std::string* error) const
    {return WriteFields(nullptr,stream,base,error);}
    bool spParticleSystemSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& base,std::string* error) const
    {return WriteFields(&manager,stream,base,error);}
    bool spParticleSystemSerializer::WriteFields(spSerializerManager* manager,spStream& stream,const spBaseObject& base,std::string* error) const
    {
        if(error)error->clear();const auto* object=dynamic_cast<const spParticleSystem*>(&base);
        if(!object){if(error)*error="Particle writer type mismatch";return false;}
        const auto& p=object->Parameters();const auto node=object->GetRenderNodeForAnalysis();
        if(!Finite(p)||p.regionType>7||(!manager&&node))
        {if(error)*error="Invalid Particle parameters or missing graph manager";return false;}
        unsigned region=0;while(region<7&&RegionTypes[region]!=p.regionType)++region;
        if((p.regionType&&p.region.size()!=RegionSizes[region])||(!p.regionType&&!p.region.empty()))
        {if(error)*error="Particle region type/extent mismatch";return false;}
        if(!WriteRenderableFieldsForAnalysis(manager,stream,*object,error))return false;
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,object))return false;
        const auto field=[&](std::uint32_t id,const auto& value){return blocks.WriteFieldForAnalysis(stream,id,&value,sizeof(value));};
        const auto reserved=[&](std::uint32_t id,const void* value,std::uint32_t size,spDataBlockSerializer::SizeCode code)
        {return blocks.WriteBeginForAnalysis(id,code)&&stream.WriteData(value,size)&&blocks.WriteEndForAnalysis(id);};
        using Code=spDataBlockSerializer::SizeCode;
        if((Different(p.acceleration[0],{0,0,1})||Different(p.acceleration[1],{0,0,1}))
            &&!reserved(0,p.acceleration.data(),24,Code::UInt8))return false;
        if(Different(p.direction,{0,0,0})&&!field(1,p.direction))return false;
        if((p.velocity[0]!=0||!BitsEqual(p.velocity[1],1))&&!field(2,p.velocity))return false;
        if((p.angle[0]!=0||!BitsEqual(p.angle[1],180))&&!field(3,p.angle))return false;
        if((!BitsEqual(p.scale[0],1)||!BitsEqual(p.scale[1],1))&&!field(4,p.scale))return false;
        if((p.colors[0]!=0xffffffff||p.colors[1])&&!reserved(5,p.colors.data(),8,Code::UInt8))return false;
        if((!BitsEqual(p.times[0],-1)||!BitsEqual(p.times[1],1))&&!field(6,p.times))return false;
        if(!p.flags[0]&&!field(7,p.flags[0]))return false;
        if(!p.flags[1]&&!field(8,p.flags[1]))return false;
        if(p.flags[2]&&!field(9,p.flags[2]))return false;
        if(!BitsEqual(p.rate,100)&&!field(10,p.rate))return false;
        if(!field(11,p.sphere[3]))return false;
        if(p.regionType&&!reserved(region+12,p.region.data(),std::uint32_t(p.region.size()*4),Code::UInt8))return false;
        if(node&&(!blocks.WriteBeginForAnalysis(19,Code::UInt32)||!WriteReferenceForAnalysis(*manager,stream,node.get(),error)
            ||!blocks.WriteEndForAnalysis(19)))return false;
        return blocks.FinalizeObjectForAnalysis();
    }
}
