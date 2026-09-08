#include "spCollisionInfoSerializer.h"
#include "spCollisionInfo.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spAnimationMath.h"
#include <algorithm>
#include <cmath>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spCollisionInfoSerializer>();}
        const spRTTIRecord Record{spCollisionInfoSerializer::ClassID,spSerializer::ClassID,"spCollisionInfoSerializer",
            &spSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        template<class T>bool Finite(const T& values)
        {return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});}
    }
    const spRTTIRecord& spCollisionInfoSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spCollisionInfoSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spCollisionInfoSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spCollisionInfoSerializer>();manager.RegisterClone(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spCollisionInfoSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
        auto* info=dynamic_cast<spCollisionInfo*>(&object);
        if(!info)return cursor.Fail("CollisionInfo target mismatch");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            switch(static_cast<Field>(header->fieldID))
            {
            case Field::Primitive:
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spBoundingVolume::ClassID,stream,*header,error);
                if(context.failed)return false;
                auto primitive=std::dynamic_pointer_cast<spBoundingVolume>(context.ShareObjectForAnalysis(raw));
                if(raw&&!primitive)return cursor.Fail("Bounding volume lacks a typed shared owner");
                info->SetPrimitiveForAnalysis(std::move(primitive));break;
            }
            case Field::Group:
            {
                std::uint32_t group=0;if(!cursor.Read(group))return cursor.Fail("Invalid CollisionInfo group");
                info->SetGroupForAnalysis(group);break;
            }
            case Field::Transform:
            {
                std::array<float,10> value{};
                if(!cursor.Read(value)||!Finite(value))return cursor.Fail("Invalid CollisionInfo transform");
                info->SetTransformForAnalysis({value[0],value[1],value[2]},
                    evidence::pc::animation_math::ToMatrix({value[3],value[4],value[5],value[6]}),
                    {value[7],value[8],value[9]});break;
            }
            default:if(!cursor.Skip())return cursor.Fail("Cannot skip CollisionInfo field");break;
            }
        }
        return false;
    }
    bool spCollisionInfoSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* info=dynamic_cast<spCollisionInfo*>(&object);
        return info&&IndexReferenceForAnalysis(manager,info->GetPrimitiveForAnalysis());
    }
    bool spCollisionInfoSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {return WriteFields(nullptr,stream,object,error);}
    bool spCollisionInfoSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,
        const spBaseObject& object,std::string* error) const{return WriteFields(&manager,stream,object,error);}
    bool spCollisionInfoSerializer::WriteFields(spSerializerManager* manager,spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* message){if(error)*error=message;return false;};
        const auto* info=dynamic_cast<const spCollisionInfo*>(&object);
        if(!info)return fail("CollisionInfo write target mismatch");
        if(!Finite(info->GetPositionForAnalysis())||!Finite(info->GetOrientationForAnalysis())||!Finite(info->GetScaleForAnalysis()))
            return fail("Nonfinite CollisionInfo transform cannot be safely written");
        if(info->GetPrimitiveForAnalysis()&&!manager)return fail("Collision primitive write requires serializer manager");
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&object))return fail("Cannot begin CollisionInfo section");
        if(info->GetPrimitiveForAnalysis()&&(!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32)
            ||!WriteReferenceForAnalysis(*manager,stream,info->GetPrimitiveForAnalysis(),error)||!blocks.WriteEndForAnalysis(0)))return false;
        const auto group=info->GetGroupForAnalysis();
        if(!blocks.WriteFieldForAnalysis(stream,1,&group,sizeof(group)))return fail("Cannot write collision group");
        const auto q=evidence::pc::animation_math::FromMatrix(info->GetOrientationForAnalysis());
        if(!Finite(q))return fail("Invalid CollisionInfo rotation conversion");
        const auto& p=info->GetPositionForAnalysis();const auto& s=info->GetScaleForAnalysis();
        const std::array<float,10> transform{p[0],p[1],p[2],q[0],q[1],q[2],q[3],s[0],s[1],s[2]};
        return blocks.WriteFieldForAnalysis(stream,2,transform.data(),sizeof(transform))&&blocks.FinalizeObjectForAnalysis();
    }
}
