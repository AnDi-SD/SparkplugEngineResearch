#include "spUVControllerSerializer.h"
#include "spUVController.h"
#include "spTransFunctionEvalSerializer.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateUVControllerSerializer()
        {
            return std::make_unique<spUVControllerSerializer>();
        }

        const spRTTIRecord UVControllerSerializerRecord{
            spUVControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spUVControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateUVControllerSerializer,
            nullptr,
        };

        const bool UVControllerSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(UVControllerSerializerRecord);
    }

    bool spUVControllerSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role
            && transformOffset == other.transformOffset
            && targetOffset == other.targetOffset;
    }

    bool spUVControllerSerializer::VectorBinding::operator==(
        const VectorBinding& other) const noexcept
    {
        return role == other.role
            && transformOffset == other.transformOffset
            && targetOffset == other.targetOffset;
    }

    spUVControllerSerializer::spUVControllerSerializer() noexcept{(void)spUVController::StaticRTTI();}
    spUVControllerSerializer::~spUVControllerSerializer() = default;

    bool spUVControllerSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* uv=dynamic_cast<spUVController*>(&object);
        evidence::pc::serialization::SectionCursor cursor(context,source,size,true,error);
        if(!uv)return cursor.Fail("UV controller target mismatch");
        while(const auto* field=cursor.Next())
        {
            if(field->IsTerminator())return true;
            if(field->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip UV field");continue;}
            spTransFunctionEvalSerializer nested;
            if(!nested.ReadPayloadForAnalysis(context,source,field->payloadSize,uv->GetTransformForAnalysis(),error))return false;
        }
        return false;
    }
    bool spUVControllerSerializer::WritePayloadForAnalysis(spStream& output,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* uv=dynamic_cast<const spUVController*>(&object);
        if(!uv){if(error)*error="UV controller writer target mismatch";return false;}
        spDataBlockSerializer blocks;spTransFunctionEvalSerializer nested;
        if(blocks.BeginObjectForAnalysis(output,uv)&&blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32)
            &&nested.WritePayloadForAnalysis(output,uv->GetTransformForAnalysis(),error)
            &&blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis())return true;
        if(error&&error->empty())*error="Cannot finish UV controller section";return false;
    }
    bool spUVControllerSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return dynamic_cast<spUVController*>(&object)!=nullptr;}

    const spRTTIRecord& spUVControllerSerializer::StaticRTTI() noexcept
    {
        (void)UVControllerSerializerRegistered;
        return UVControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spUVControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spUVControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spUVControllerSerializer::vfunc_18() const noexcept
    {
        return UVControllerSerializerRecord;
    }

    spClassID spUVControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spUVControllerSerializer::Field>
    spUVControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::UVController};
    }

    spUVControllerSerializer::EvaluatorPlan
    spUVControllerSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::TranslationX, 0x10, 0x5C},
            {EvaluatorRole::TranslationY, 0x48, 0x94},
            {EvaluatorRole::TranslationZ, 0x80, 0xCC},
            {EvaluatorRole::ScaleX, 0xB8, 0x104},
            {EvaluatorRole::ScaleY, 0xF0, 0x13C},
            {EvaluatorRole::ScaleZ, 0x128, 0x174},
            {EvaluatorRole::Rotation, 0x178, 0x1C4},
        }};
    }

    spUVControllerSerializer::VectorPlan
    spUVControllerSerializer::BuildVectorPlanForAnalysis() noexcept
    {
        return {{
            {VectorRole::UVPivot, 0x160, 0x1AC},
            {VectorRole::RotationAxis, 0x16C, 0x1B8},
        }};
    }

    bool spUVControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::UVController);
    }
}
