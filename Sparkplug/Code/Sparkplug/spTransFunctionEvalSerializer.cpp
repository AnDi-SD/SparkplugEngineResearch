#include "spTransFunctionEvalSerializer.h"
#include "spTransFunctionEval.h"
#include "spFunctionEvalSerializer.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTransFunctionEvalSerializer()
        {
            return std::make_unique<spTransFunctionEvalSerializer>();
        }

        const spRTTIRecord TransFunctionEvalSerializerRecord{
            spTransFunctionEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spTransFunctionEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateTransFunctionEvalSerializer,
            nullptr,
        };

        const bool TransFunctionEvalSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TransFunctionEvalSerializerRecord);
    }

    bool spTransFunctionEvalSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role && targetOffset == other.targetOffset;
    }

    bool spTransFunctionEvalSerializer::VectorBinding::operator==(
        const VectorBinding& other) const noexcept
    {
        return role == other.role && targetOffset == other.targetOffset;
    }

    spTransFunctionEvalSerializer::spTransFunctionEvalSerializer() noexcept{(void)spTransFunctionEval::StaticRTTI();}
    spTransFunctionEvalSerializer::~spTransFunctionEvalSerializer() = default;

    bool spTransFunctionEvalSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* transform=dynamic_cast<spTransFunctionEval*>(&object);
        evidence::pc::serialization::SectionCursor cursor(context,source,size,true,error);
        if(!transform)return cursor.Fail("TransFunctionEval target mismatch");
        while(const auto* field=cursor.Next())
        {
            if(field->IsTerminator())return true;
            if(field->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip TransFunction field");continue;}
            const auto end=field->dataStreamPosition+field->payloadSize;
            spFunctionEvalSerializer nested;
            for(auto& function:transform->GetFunctionsForAnalysis())
            {
                std::uint32_t pos=0;if(!source.GetCurrentPosition(pos)||pos>=end)return cursor.Fail("Missing packed Function section");
                if(!nested.ReadFunctionFieldsForAnalysis(context,source,end-pos,function,false,error))return false;
            }
            std::uint32_t pos=0;spTransformEval::Vector3 pivot{},axis{};
            if(!source.GetCurrentPosition(pos)||pos>end||end-pos!=24||!source.ReadData(pivot.data(),12)||!source.ReadData(axis.data(),12))
                return cursor.Fail("TransFunction requires pivot and axis after seven sections");
            transform->SetPivotForAnalysis(pivot);transform->SetAxisForAnalysis(axis);
        }
        return false;
    }
    bool spTransFunctionEvalSerializer::WritePayloadForAnalysis(spStream& output,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* transform=dynamic_cast<const spTransFunctionEval*>(&object);
        if(!transform){if(error)*error="TransFunction writer target mismatch";return false;}
        // UInt8 reservation is the original writer; no silently enlarged
        // field for unusual oversized scalar combinations.
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(output,transform)||!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt8))return false;
        spFunctionEvalSerializer nested;
        for(const auto& function:transform->GetFunctionsForAnalysis())if(!nested.WritePayloadForAnalysis(output,function,error))return false;
        if(!output.WriteData(transform->GetPivotForAnalysis().data(),12)||!output.WriteData(transform->GetAxisForAnalysis().data(),12)
            ||!blocks.WriteEndForAnalysis(0)||!blocks.FinalizeObjectForAnalysis())
        {if(error)*error="Cannot write bounded TransFunction field";return false;}return true;
    }
    bool spTransFunctionEvalSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return dynamic_cast<spTransFunctionEval*>(&object)!=nullptr;}

    const spRTTIRecord& spTransFunctionEvalSerializer::StaticRTTI() noexcept
    {
        (void)TransFunctionEvalSerializerRegistered;
        return TransFunctionEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTransFunctionEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTransFunctionEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTransFunctionEvalSerializer::vfunc_18() const noexcept
    {
        return TransFunctionEvalSerializerRecord;
    }

    spClassID
    spTransFunctionEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spTransFunctionEvalSerializer::Field>
    spTransFunctionEvalSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::TransformFunctions};
    }

    spTransFunctionEvalSerializer::EvaluatorPlan
    spTransFunctionEvalSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::TranslationX, 0x10},
            {EvaluatorRole::TranslationY, 0x48},
            {EvaluatorRole::TranslationZ, 0x80},
            {EvaluatorRole::ScaleX, 0xB8},
            {EvaluatorRole::ScaleY, 0xF0},
            {EvaluatorRole::ScaleZ, 0x128},
            {EvaluatorRole::Rotation, 0x178},
        }};
    }

    spTransFunctionEvalSerializer::VectorPlan
    spTransFunctionEvalSerializer::BuildVectorPlanForAnalysis() noexcept
    {
        return {{
            {VectorRole::UVPivot, 0x160},
            {VectorRole::RotationAxis, 0x16C},
        }};
    }

    bool spTransFunctionEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::TransformFunctions);
    }
}
