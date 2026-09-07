#include "spMatColorControllerSerializer.h"
#include "spMaterialColorController.h"
#include "spColorFuncEvalSerializer.h"
#include "spFunctionEvalSerializer.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMatColorControllerSerializer()
        {
            return std::make_unique<spMatColorControllerSerializer>();
        }

        const spRTTIRecord MatColorControllerSerializerRecord{
            spMatColorControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spMatColorControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateMatColorControllerSerializer,
            nullptr,
        };

        const bool MatColorControllerSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MatColorControllerSerializerRecord);
    }

    bool spMatColorControllerSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role
            && kind == other.kind
            && targetOffset == other.targetOffset;
    }

    spMatColorControllerSerializer::spMatColorControllerSerializer() noexcept{(void)spMaterialColorController::StaticRTTI();}
    spMatColorControllerSerializer::~spMatColorControllerSerializer() = default;

    bool spMatColorControllerSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* controller=dynamic_cast<spMaterialColorController*>(&object);
        evidence::pc::serialization::SectionCursor cursor(context,source,size,true,error);
        if(!controller)return cursor.Fail("MaterialColorController target mismatch");
        while(const auto* field=cursor.Next())
        {
            if(field->IsTerminator())return true;
            if(field->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip material color field");continue;}
            const auto end=field->dataStreamPosition+field->payloadSize;spColorFuncEvalSerializer colorCodec;spFunctionEvalSerializer scalarCodec;
            for(auto& color:controller->GetColorsForAnalysis())
            {
                std::uint32_t pos=0;if(!source.GetCurrentPosition(pos)||pos>=end)return cursor.Fail("Missing packed ColorFunc section");
                if(!colorCodec.ReadColorFieldsForAnalysis(context,source,end-pos,color,false,error))return false;
            }
            std::uint32_t pos=0;if(!source.GetCurrentPosition(pos)||pos>=end)return cursor.Fail("Missing packed alpha Function section");
            if(!scalarCodec.ReadFunctionFieldsForAnalysis(context,source,end-pos,controller->GetAlphaForAnalysis(),true,error))return false;
        }
        return false;
    }
    bool spMatColorControllerSerializer::WritePayloadForAnalysis(spStream& output,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* controller=dynamic_cast<const spMaterialColorController*>(&object);
        if(!controller){if(error)*error="MaterialColorController writer target mismatch";return false;}
        spDataBlockSerializer blocks;spColorFuncEvalSerializer colorCodec;spFunctionEvalSerializer scalarCodec;
        if(!blocks.BeginObjectForAnalysis(output,controller)||!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32))return false;
        for(const auto& color:controller->GetColorsForAnalysis())if(!colorCodec.WritePayloadForAnalysis(output,color,error))return false;
        if(scalarCodec.WritePayloadForAnalysis(output,controller->GetAlphaForAnalysis(),error)&&blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis())return true;
        if(error&&error->empty())*error="Cannot finish material color section";return false;
    }
    bool spMatColorControllerSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return dynamic_cast<spMaterialColorController*>(&object)!=nullptr;}

    const spRTTIRecord& spMatColorControllerSerializer::StaticRTTI() noexcept
    {
        (void)MatColorControllerSerializerRegistered;
        return MatColorControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMatColorControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMatColorControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMatColorControllerSerializer::vfunc_18() const noexcept
    {
        return MatColorControllerSerializerRecord;
    }

    spClassID
    spMatColorControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spMatColorControllerSerializer::Field>
    spMatColorControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::MaterialColorController};
    }

    spMatColorControllerSerializer::EvaluatorPlan
    spMatColorControllerSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::Ambient, EvaluatorKind::ColorFunctional, 0x68},
            {EvaluatorRole::Diffuse, EvaluatorKind::ColorFunctional, 0xB8},
            {EvaluatorRole::Specular, EvaluatorKind::ColorFunctional, 0x108},
            {EvaluatorRole::Emissive, EvaluatorKind::ColorFunctional, 0x158},
            {EvaluatorRole::Alpha, EvaluatorKind::Functional, 0x1A8},
        }};
    }

    bool spMatColorControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID
            == static_cast<std::uint32_t>(Field::MaterialColorController);
    }
}
