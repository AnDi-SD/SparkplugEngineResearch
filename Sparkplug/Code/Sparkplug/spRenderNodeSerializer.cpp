#include "spRenderNodeSerializer.h"

#include "spRenderable.h"
#include "spRenderNode.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderNodeSerializer()
        {
            return std::make_unique<spRenderNodeSerializer>();
        }

        const spRTTIRecord RenderNodeSerializerRecord{
            spRenderNodeSerializer::ClassID,
            spNodeSerializer::ClassID,
            "spRenderNodeSerializer",
            &spNodeSerializer::StaticRTTI(),
            &CreateRenderNodeSerializer,
            nullptr,
        };

        const bool RenderNodeSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(RenderNodeSerializerRecord);
    }

    spRenderNodeSerializer::~spRenderNodeSerializer() = default;

    const spRTTIRecord& spRenderNodeSerializer::StaticRTTI() noexcept
    {
        (void)RenderNodeSerializerRegistered;
        return RenderNodeSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spRenderNodeSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderNodeSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spRenderNodeSerializer::vfunc_18() const noexcept
    {
        return RenderNodeSerializerRecord;
    }

    spClassID spRenderNodeSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spRenderNode::ClassID;
    }

    spRenderNodeSerializer::WritePlanForAnalysis
    spRenderNodeSerializer::BuildKnownWritePlanForAnalysis(
        const spRenderNode& node) const
    {
        WritePlanForAnalysis plan;
        plan.nodeFields =
            spNodeSerializer::BuildKnownWritePlanForAnalysis(node);
        plan.renderables.reserve(node.GetRenderableCountForAnalysis());
        for (std::size_t index = 0;
             index < node.GetRenderableCountForAnalysis(); ++index)
        {
            if (const auto* renderable =
                    node.GetRenderableForAnalysis(index))
            {
                plan.renderables.push_back(renderable);
            }
        }
        return plan;
    }

    bool spRenderNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        auto* node=dynamic_cast<spRenderNode*>(&object);
        if(!node||!object.IsExactly(spRenderNode::ClassID))
        {context.failed=true;if(error)*error="RenderNode section target mismatch";return false;}
        std::uint32_t start=0,position=0;
        if(!source.GetCurrentPosition(start)||!ReadNodeFieldsForAnalysis(context,source,size,*node,false,error)
            ||!source.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing RenderNode derived section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,source,size-(position-start),true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip RenderNode field");continue;}
            auto* relationship=ReadFieldReferenceForAnalysis(context,spRenderable::ClassID,source,*header,error);
            if(context.failed)return false;
            if(!AttachResolvedRenderableForAnalysis(*node,context.ShareObjectForAnalysis(relationship)))
                return cursor.Fail("RenderNode renderable is null, wrong type or lacks an explicit owner");
        }
        return false;
    }

    bool spRenderNodeSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* node=dynamic_cast<spRenderNode*>(&object);if(!node||!object.IsExactly(spRenderNode::ClassID))return false;
        for(std::size_t i=0;i<node->GetChildCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,node->GetChildForAnalysis(i)))return false;
        for(std::size_t i=0;i<node->GetRenderableCountForAnalysis();++i)
            if(!IndexReferenceForAnalysis(manager,node->GetRenderableForAnalysis(i)))return false;
        return true;
    }

    bool spRenderNodeSerializer::WriteSectionsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spRenderNode& node,std::string* error) const
    {
        if(error)error->clear();
        if(!manager&&node.GetRenderableCountForAnalysis())
        {if(error)*error="RenderNode graph write requires explicit serializer manager";return false;}
        if(!WriteNodeFieldsForAnalysis(manager,stream,node,error))return false;
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&node))return false;
        for(std::size_t i=0;i<node.GetRenderableCountForAnalysis();++i)
            if(!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32)
                ||!WriteReferenceForAnalysis(*manager,stream,node.GetRenderableForAnalysis(i),error)
                ||!blocks.WriteEndForAnalysis(0))return false;
        return blocks.FinalizeObjectForAnalysis();
    }

    bool spRenderNodeSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* node=dynamic_cast<const spRenderNode*>(&object);
        if(!node||!object.IsExactly(spRenderNode::ClassID)){if(error)*error="RenderNode write target mismatch";return false;}
        return WriteSectionsForAnalysis(nullptr,stream,*node,error);
    }

    bool spRenderNodeSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* node=dynamic_cast<const spRenderNode*>(&object);
        if(!node||!object.IsExactly(spRenderNode::ClassID)){if(error)*error="RenderNode write target mismatch";return false;}
        return WriteSectionsForAnalysis(&manager,stream,*node,error);
    }

    bool spRenderNodeSerializer::AttachResolvedRenderableForAnalysis(
        spRenderNode& node,
        std::shared_ptr<spBaseObject> relationship) const
    {
        auto renderable = std::dynamic_pointer_cast<spRenderable>(
            std::move(relationship));
        return renderable != nullptr
            && node.AttachRenderableForAnalysis(std::move(renderable));
    }
}
