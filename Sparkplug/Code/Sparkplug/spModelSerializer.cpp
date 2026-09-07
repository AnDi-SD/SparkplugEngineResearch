#include "spModelSerializer.h"
#include "spMesh.h"
#include "Analysis/PC/spSectionCursor.h"

#include "spModel.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateModelSerializer()
        {
            return std::make_unique<spModelSerializer>();
        }

        const spRTTIRecord ModelSerializerRecord{
            spModelSerializer::ClassID,
            spRenderableSerializer::ClassID,
            "spModelSerializer",
            &spRenderableSerializer::StaticRTTI(),
            &CreateModelSerializer,
            nullptr,
        };

        const bool ModelSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(ModelSerializerRecord);
    }

    bool spModelSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return renderableFields == other.renderableFields
            && modelFields == other.modelFields;
    }

    spModelSerializer::~spModelSerializer() = default;

    const spRTTIRecord& spModelSerializer::StaticRTTI() noexcept
    {
        (void)ModelSerializerRegistered;
        return ModelSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spModelSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spModelSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spModelSerializer::vfunc_18() const noexcept
    {
        return ModelSerializerRecord;
    }

    spClassID spModelSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spModel::ClassID;
    }

    bool spModelSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* model=dynamic_cast<spModel*>(&object);
        if(!model||!object.IsExactly(spModel::ClassID)||GetTargetClassIDForAnalysis()!=spModel::ClassID)
        {context.failed=true;if(error)*error="Derived Model serializer requires its own section adapter";return false;}
        return ReadModelFieldsForAnalysis(context,stream,size,*model,true,error);
    }

    bool spModelSerializer::ReadModelFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spModel& object,bool requireEnd,std::string* error) const
    {
        auto* model=&object;
        std::uint32_t start=0,position=0;
        if(!stream.GetCurrentPosition(start)||!ReadRenderableFieldsForAnalysis(context,stream,size,*model,false,error)
            ||!stream.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing Model section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,stream,size-(position-start),requireEnd,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator()){model->InvalidateRuntimeModeForAnalysis();return true;}
            if(header->fieldID==0)
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spMesh::ClassID,stream,*header,error);
                if(context.failed)return false;
                auto mesh=std::dynamic_pointer_cast<spMesh>(context.ShareObjectForAnalysis(raw));
                if(!mesh)return cursor.Fail("Model mesh is null, wrong type or lacks an explicit owner");
                model->SetBaseMeshForAnalysis(std::move(mesh));
            }
            else if(header->fieldID==1)
            {
                std::uint32_t value=0;
                // Native493973 does not test Read result; host validates it.
                if(!cursor.Read(value))return cursor.Fail("Invalid Model projection-group UInt32");
                model->SetProjectionGroupForAnalysis(value);
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip Model field");
        }
        return false;
    }

    bool spModelSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        auto* model=dynamic_cast<spModel*>(&object);
        return model&&object.IsExactly(spModel::ClassID)&&GetTargetClassIDForAnalysis()==spModel::ClassID
            &&IndexModelFieldsForAnalysis(manager,*model);
    }

    bool spModelSerializer::IndexModelFieldsForAnalysis(spSerializerManager& manager,spModel& model) const
    {
        return IndexRenderableFieldsForAnalysis(manager,model)
            &&IndexReferenceForAnalysis(manager,model.GetBaseMeshForAnalysis().get());
    }

    bool spModelSerializer::WriteModelFieldsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spModel& model,std::string* error) const
    {
        if(error)error->clear();
        if(!manager&&model.GetBaseMeshForAnalysis()){if(error)*error="Model graph writer requires explicit manager";return false;}
        if(!WriteRenderableFieldsForAnalysis(manager,stream,model,error))return false;
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&model))return false;
        if(model.GetBaseMeshForAnalysis())
            if(!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32)
                ||!WriteReferenceForAnalysis(*manager,stream,model.GetBaseMeshForAnalysis().get(),error)
                ||!blocks.WriteEndForAnalysis(0))return false;
        // Original resolved writer uses Begin/End even for the four-byte
        // scalar. Preserve e1/UInt32 length framing, not minimal61 encoding.
        const auto group=model.GetProjectionGroupForAnalysis();
        return blocks.WriteBeginForAnalysis(1,spDataBlockSerializer::SizeCode::UInt32)
            &&stream.WriteData(&group,sizeof(group))&&blocks.WriteEndForAnalysis(1)&&blocks.FinalizeObjectForAnalysis();
    }

    bool spModelSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* model=dynamic_cast<const spModel*>(&object);
        if(!model||!object.IsExactly(spModel::ClassID)||GetTargetClassIDForAnalysis()!=spModel::ClassID)
        {if(error)*error="Derived Model writer requires its own section adapter";return false;}
        return WriteModelFieldsForAnalysis(nullptr,stream,*model,error);
    }

    bool spModelSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* model=dynamic_cast<const spModel*>(&object);
        if(!model||!object.IsExactly(spModel::ClassID)||GetTargetClassIDForAnalysis()!=spModel::ClassID)
        {if(error)*error="Derived Model writer requires its own section adapter";return false;}
        return WriteModelFieldsForAnalysis(&manager,stream,*model,error);
    }

    spModelSerializer::KnownWritePlan
    spModelSerializer::BuildKnownWritePlanForAnalysis(const spModel& model) const
    {
        KnownWritePlan plan;
        plan.renderableFields =
            spRenderableSerializer::BuildKnownWritePlanForAnalysis(model);
        if (model.GetBaseMeshForAnalysis())
        {
            plan.modelFields.push_back(Field::Base);
        }
        plan.modelFields.push_back(Field::ProjectionGroup);
        return plan;
    }
}
