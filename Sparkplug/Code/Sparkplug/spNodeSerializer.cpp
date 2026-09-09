#include "spNodeSerializer.h"

#include "spNode.h"
#include "spCollisionInfo.h"
#include "spDataBlockSerializer.h"
#include "spSerializerManager.h"
#include "../../Analysis/PC/spAnimationMath.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateNodeSerializer()
        {
            return std::make_unique<spNodeSerializer>();
        }

        const spRTTIRecord NodeSerializerRecord{
            spNodeSerializer::ClassID,
            spSerializer::ClassID,
            "spNodeSerializer",
            &spSerializer::StaticRTTI(),
            &CreateNodeSerializer,
            nullptr,
        };

        const bool NodeSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(NodeSerializerRecord);

        template <std::size_t Size>
        bool ApproximatelyEqual(
            const std::array<float, Size>& left,
            const std::array<float, Size>& right) noexcept
        {
            for (std::size_t index = 0; index < Size; ++index)
            {
                if (std::fabs(left[index] - right[index])
                    > spNodeSerializer::DefaultComparisonTolerance)
                {
                    return false;
                }
            }
            return true;
        }
    }

    spNodeSerializer::~spNodeSerializer() = default;

    const spRTTIRecord& spNodeSerializer::StaticRTTI() noexcept
    {
        (void)NodeSerializerRegistered;
        return NodeSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spNodeSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spNodeSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spNodeSerializer::vfunc_18() const noexcept
    {
        return NodeSerializerRecord;
    }

    spClassID spNodeSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spNode::ClassID;
    }

    std::vector<spNodeSerializer::Field>
    spNodeSerializer::BuildKnownWritePlanForAnalysis(const spNode& node) const
    {
        static constexpr spNode::Vector3 ZeroVector{0.0F, 0.0F, 0.0F};
        static constexpr spNode::Vector3 UnitVector{1.0F, 1.0F, 1.0F};
        static constexpr spNode::Matrix3 IdentityMatrix{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };

        std::vector<Field> fields;
        if (!ApproximatelyEqual(node.GetPositionForAnalysis(), ZeroVector))
        {
            fields.push_back(Field::Position);
        }
        if (!ApproximatelyEqual(node.GetOrientationForAnalysis(), IdentityMatrix))
        {
            fields.push_back(Field::Rotation);
        }
        if (!ApproximatelyEqual(node.GetScaleForAnalysis(), UnitVector))
        {
            fields.push_back(Field::Scale);
        }
        if (node.IsBoneForAnalysis())
        {
            fields.push_back(Field::IsBone);
        }
        if (node.IsStaticForAnalysis())
        {
            fields.push_back(Field::IsStatic);
        }

        // Both native writers emit the current animated state explicitly.
        fields.push_back(Field::IsAnimated);

        for (std::size_t index = 0;
             index < node.GetChildCountForAnalysis(); ++index)
        {
            if (node.GetChildForAnalysis(index) != nullptr)
            {
                fields.push_back(Field::Child);
            }
        }

        if (node.GetBillboardAxisForAnalysis() != 0)
        {
            fields.push_back(Field::BillboardAxis);
        }

        for(std::size_t index=0;index<node.GetCollisionCountForAnalysis();++index)
            fields.push_back(Field::Collision);

        return fields;
    }

    bool spNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        if(GetTargetClassIDForAnalysis()!=spNode::ClassID||!object.IsExactly(spNode::ClassID))
        {if(error)*error="Derived Node serializer needs its own section adapter";return false;}
        auto* node=dynamic_cast<spNode*>(&object);
        if(!node){if(error)*error="Node reader requires Node-compatible object";return false;}
        return ReadNodeFieldsForAnalysis(context,source,byteCount,*node,true,error);
    }

    bool spNodeSerializer::ReadScalarFieldForAnalysis(spStream& source,
        const spDataBlockHeaderForAnalysis& header,spNode& node,bool& handled,
        ScalarObservationForAnalysis* observation,std::string* error)
    {
        handled=header.fieldID<=8&&header.fieldID!=5&&header.fieldID!=7;
        if(!handled)return true;
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        const auto read=[&](auto& value){return header.payloadSize==sizeof(value)&&source.ReadData(&value,sizeof(value));};
        switch(static_cast<Field>(header.fieldID))
        {
            case Field::Position:case Field::Scale:
            {
                spNode::Vector3 value{};
                if(!read(value)||!std::all_of(value.begin(),value.end(),[](float v){return std::isfinite(v);}))
                    return fail("Invalid Node vector payload");
                if(header.fieldID==0)node.SetPositionForAnalysis(value);else node.SetScaleForAnalysis(value);
                node.MarkLocalTransformDirtyForAnalysis();break;
            }
            case Field::Rotation:
            {
                evidence::pc::animation_math::Quaternion value{};
                if(!read(value)||!std::all_of(value.begin(),value.end(),[](float v){return std::isfinite(v);}))
                    return fail("Invalid Node quaternion payload");
                node.SetOrientationForAnalysis(evidence::pc::animation_math::ToMatrix(value));
                if(observation)observation->rotation=value;
                node.MarkLocalTransformDirtyForAnalysis();break;
            }
            case Field::IsBone:case Field::IsStatic:case Field::IsAnimated:
            {
                std::uint8_t value=0;if(!read(value))return fail("Invalid Node flag payload");
                if(header.fieldID==3)node.SetBoneForAnalysis(value!=0);
                else if(value&&header.fieldID==4)node.SetStaticForAnalysis(true);
                else if(value)node.SetAnimatedForAnalysis(true);
                if(observation) {
                    if(header.fieldID==3)observation->bone=value;
                    else if(header.fieldID==4)observation->isStatic=value;
                    else observation->animated=value;
                }
                // Original reader intentionally does NOT clear Static/Animated
                // on false. A fresh default Node keeps Animated even from0.
                break;
            }
            case Field::BillboardAxis:
            {
                std::uint32_t value=0;if(!read(value))return fail("Invalid Node billboard field");
                node.SetBillboardAxisForAnalysis(value);break;
            }
        default:break;
        }
        return true;
    }

    bool spNodeSerializer::ReadNodeFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spNode& node,bool requireExactEnd,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){context.failed=true;if(error)*error=message;return false;};
        std::uint32_t start=0,size=0;
        if(context.failed||!byteCount||byteCount>16u*1024u*1024u||!source.GetCurrentPosition(start)
            ||!source.GetSize(&size)||source.GetLogicalOriginForAnalysis()>size)
            return fail("Invalid bounded Node section");
        size-=source.GetLogicalOriginForAnalysis();
        if(start>size||byteCount>size-start)return fail("Node section exceeds logical segment");
        const auto end=start+byteCount;
        spDataBlockSerializer blocks;
        for(std::uint32_t fields=0;fields<65536;++fields)
        {
            std::uint32_t position=0;
            if(!source.GetCurrentPosition(position)||position>=end)return fail("Missing Node section terminator");
            const auto* header=blocks.ReadHeaderForAnalysis(source);
            if(!header||header->dataStreamPosition>end||header->payloadSize>end-header->dataStreamPosition)
                return fail("Truncated Node field");
            if(header->IsTerminator())
            {
                if(requireExactEnd&&header->dataStreamPosition!=end)return fail("Trailing Node section bytes");
                // Actual463C95 calls virtual world update with inherited2.
                // A null camera is the confirmed native identity-billboard case.
                // An explicit degenerate/nonfinite camera is a host rejection.
                return node.UpdateWorldForAnalysis(2,context.cameraOrientation)
                    ?true:fail("Node world update rejected a camera basis or unresolved dependency");
            }
            bool handled=false;
            if(!ReadScalarFieldForAnalysis(source,*header,node,handled,nullptr,error)){context.failed=true;return false;}
            if(!handled)switch(static_cast<Field>(header->fieldID))
            {
            case Field::Child:
            {
                auto* relationship=ReadFieldReferenceForAnalysis(context,spNode::ClassID,source,*header,error);
                if(context.failed)return false;
                auto child=std::dynamic_pointer_cast<spNode>(context.ShareObjectForAnalysis(relationship));
                if(!child)return fail("Node child is null, wrong type, or lacks an explicit shared owner");
                if(!child->UpdateWorldForAnalysis(2,context.cameraOrientation)||!node.AttachChildForAnalysis(child))
                    return fail("Cannot attach Node child; cycle/reparent/scene dependency remains unsupported");
                break;
            }
            case Field::Collision:
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spCollisionInfo::ClassID,source,*header,error);
                if(context.failed)return false;
                auto collision=std::dynamic_pointer_cast<spCollisionInfo>(context.ShareObjectForAnalysis(raw));
                if(!collision||!node.AttachCollisionForAnalysis(std::move(collision)))
                    return fail("Collision is null, has no owner, or is already attached");
                break;
            }
            default:
                if(!spDataBlockSerializer::SkipDataForAnalysis(source,*header))return fail("Cannot skip unknown Node field");
                break;
            }
            if(!source.GetCurrentPosition(position)||position!=header->dataStreamPosition+header->payloadSize)
                return fail("Node field did not consume its bounded extent");
        }
        return fail("Node field-count bound exceeded");
    }

    bool spNodeSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
        spBaseObject& object) const
    {
        if(GetTargetClassIDForAnalysis()!=spNode::ClassID||!object.IsExactly(spNode::ClassID))return false;
        auto* node=dynamic_cast<spNode*>(&object);if(!node)return false;
        return IndexNodeRelationshipsForAnalysis(manager,*node);
    }

    bool spNodeSerializer::IndexNodeRelationshipsForAnalysis(spSerializerManager& manager,spNode& node) const
    {
        for(std::size_t i=0;i<node.GetChildCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,node.GetChildForAnalysis(i)))return false;
        for(std::size_t i=0;i<node.GetCollisionCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,node.GetCollisionForAnalysis(i)))return false;
        return true;
    }

    bool spNodeSerializer::WritePayloadForAnalysis(spStream& destination,
        const spBaseObject& object,std::string* error) const
    {
        if(GetTargetClassIDForAnalysis()!=spNode::ClassID||!object.IsExactly(spNode::ClassID))
        {if(error)*error="Derived Node writer needs its own section adapter";return false;}
        const auto* node=dynamic_cast<const spNode*>(&object);
        if(!node){if(error)*error="Node writer requires Node-compatible object";return false;}
        return WriteNodeFieldsForAnalysis(nullptr,destination,*node,error);
    }

    bool spNodeSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& destination,const spBaseObject& object,std::string* error) const
    {
        if(GetTargetClassIDForAnalysis()!=spNode::ClassID||!object.IsExactly(spNode::ClassID))
        {if(error)*error="Derived Node writer needs its own section adapter";return false;}
        const auto* node=dynamic_cast<const spNode*>(&object);
        if(!node){if(error)*error="Node writer requires Node-compatible object";return false;}
        return WriteNodeFieldsForAnalysis(&manager,destination,*node,error);
    }

    bool spNodeSerializer::WriteNodeFieldsForAnalysis(spSerializerManager* manager,
        spStream& destination,const spNode& node,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        const auto finite=[](const auto& values){return std::all_of(values.begin(),values.end(),[](float v){return std::isfinite(v);});};
        if(!finite(node.GetPositionForAnalysis())||!finite(node.GetScaleForAnalysis())||!finite(node.GetOrientationForAnalysis()))
            return fail("Nonfinite Node transform cannot be safely written");
        if((node.GetChildCountForAnalysis()||node.GetCollisionCountForAnalysis())&&!manager)
            return fail("Node graph writer requires explicit serializer manager");
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(destination,&node))return fail("Cannot begin Node section");
        std::size_t childIndex=0,collisionIndex=0;
        for(auto field:BuildKnownWritePlanForAnalysis(node))
        {
            const auto write=[&](const auto& value){return blocks.WriteFieldForAnalysis(destination,static_cast<std::uint32_t>(field),&value,sizeof(value));};
            bool written=false;
            switch(field)
            {
            case Field::Position:written=write(node.GetPositionForAnalysis());break;
            case Field::Scale:written=write(node.GetScaleForAnalysis());break;
            case Field::Rotation:
            {
                const auto q=evidence::pc::animation_math::FromMatrix(node.GetOrientationForAnalysis());
                if(!finite(q))return fail("Invalid Node rotation conversion");
                written=write(q);break;
            }
            case Field::IsBone:written=write(std::uint8_t(node.IsBoneForAnalysis()));break;
            case Field::IsStatic:written=write(std::uint8_t(node.IsStaticForAnalysis()));break;
            case Field::IsAnimated:written=write(std::uint8_t(node.IsAnimatedForAnalysis()));break;
            case Field::BillboardAxis:written=write(node.GetBillboardAxisForAnalysis());break;
            case Field::Child:
                written=blocks.WriteBeginForAnalysis(5,spDataBlockSerializer::SizeCode::UInt32)
                    &&WriteReferenceForAnalysis(*manager,destination,node.GetChildForAnalysis(childIndex++),error)
                    &&blocks.WriteEndForAnalysis(5);break;
            case Field::Collision:
                written=blocks.WriteBeginForAnalysis(7,spDataBlockSerializer::SizeCode::UInt32)
                    &&WriteReferenceForAnalysis(*manager,destination,node.GetCollisionForAnalysis(collisionIndex++),error)
                    &&blocks.WriteEndForAnalysis(7);break;
            default:return fail("Unsupported Node write field");
            }
            if(!written){if(error&&error->empty())*error="Cannot write Node field";return false;}
        }
        return blocks.FinalizeObjectForAnalysis()?true:fail("Cannot finish Node section");
    }
}
