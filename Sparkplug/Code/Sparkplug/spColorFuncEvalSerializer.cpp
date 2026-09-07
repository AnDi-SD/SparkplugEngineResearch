#include "spColorFuncEvalSerializer.h"
#include "spColorFuncEval.h"
#include "spFunctionEvalSerializer.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateColorFuncEvalSerializer()
        {
            return std::make_unique<spColorFuncEvalSerializer>();
        }

        const spRTTIRecord ColorFuncEvalSerializerRecord{
            spColorFuncEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spColorFuncEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateColorFuncEvalSerializer,
            nullptr,
        };

        const bool ColorFuncEvalSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(ColorFuncEvalSerializerRecord);
    }

    bool spColorFuncEvalSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireKind == other.wireKind
            && defaultRule == other.defaultRule
            && targetOffset == other.targetOffset;
    }

    spColorFuncEvalSerializer::spColorFuncEvalSerializer() noexcept{(void)spColorFuncEval::StaticRTTI();}
    spColorFuncEvalSerializer::~spColorFuncEvalSerializer() = default;
    bool spColorFuncEvalSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& stream,
        std::uint32_t size,spBaseObject& object,std::string* error) const
    {return ReadColorFieldsForAnalysis(context,stream,size,object,true,error);}
    bool spColorFuncEvalSerializer::ReadColorFieldsForAnalysis(spSerializerReadContextForAnalysis& context,spStream& stream,
        std::uint32_t size,spBaseObject& object,bool exact,std::string* error) const
    {
        auto* evaluator=dynamic_cast<spColorFuncEval*>(&object);evidence::pc::serialization::SectionCursor cursor(context,stream,size,exact,error);
        if(!evaluator)return cursor.Fail("ColorFuncEval target mismatch");
        while(const auto* field=cursor.Next())
        {
            if(field->IsTerminator())return true;
            if(field->fieldID>7){if(!cursor.Skip())return cursor.Fail("Cannot skip ColorFuncEval field");continue;}
            std::uint32_t raw=0;if(!cursor.Read(raw))return cursor.Fail("ColorFuncEval field requires four bytes");
            if(field->fieldID==0)evaluator->SetColorsForAnalysis(raw,evaluator->GetColor2ForAnalysis());
            else if(field->fieldID==1)evaluator->SetColorsForAnalysis(evaluator->GetColor1ForAnalysis(),raw);
            else if(!spFunctionEvalSerializer::ApplyRawStateFieldForAnalysis(evaluator->GetFunctionForAnalysis(),field->fieldID-2,raw))return cursor.Fail("Invalid ColorFunc scalar field");
        }
        return false;
    }
    bool spColorFuncEvalSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* evaluator=dynamic_cast<const spColorFuncEval*>(&object);
        if(!evaluator){if(error)*error="ColorFuncEval writer target mismatch";return false;}
        const auto& state=evaluator->GetFunctionForAnalysis().GetStateForAnalysis();
        const std::uint32_t colors[]{evaluator->GetColor1ForAnalysis(),evaluator->GetColor2ForAnalysis()};
        const float values[]{state.frequency,state.amplitude,state.xOffset,state.yOffset,state.pitch};
        const auto plan=BuildWritePlanForAnalysis({colors[0],colors[1],state.functionType,state.frequency,state.amplitude,state.xOffset,state.yOffset,state.pitch},0xFF000000u);
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,evaluator))return false;
        for(const auto field:plan)
        {
            const auto id=static_cast<std::uint32_t>(field);const void* data=id<2?static_cast<const void*>(&colors[id]):id==2?static_cast<const void*>(&state.functionType):static_cast<const void*>(&values[id-3]);
            if(!blocks.WriteFieldForAnalysis(stream,id,data,4)){if(error)*error="Cannot write ColorFuncEval field";return false;}
        }
        if(blocks.FinalizeObjectForAnalysis())return true;
        if(error)*error="Cannot finish ColorFuncEval section";return false;
    }
    bool spColorFuncEvalSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return dynamic_cast<spColorFuncEval*>(&object)!=nullptr;}

    const spRTTIRecord& spColorFuncEvalSerializer::StaticRTTI() noexcept
    {
        (void)ColorFuncEvalSerializerRegistered;
        return ColorFuncEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spColorFuncEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spColorFuncEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spColorFuncEvalSerializer::vfunc_18() const noexcept
    {
        return ColorFuncEvalSerializerRecord;
    }

    spClassID spColorFuncEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spColorFuncEvalSerializer::FieldSchema
    spColorFuncEvalSerializer::BuildFieldSchemaForAnalysis() noexcept
    {
        return {{
            {Field::Color1, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x10},
            {Field::Color2, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x14},
            {Field::FunctionType, WireKind::UInt32, DefaultRule::Zero, 0x4C},
            {Field::Frequency, WireKind::Float32, DefaultRule::One, 0x2C},
            {Field::Amplitude, WireKind::Float32, DefaultRule::One, 0x34},
            {Field::XOffset, WireKind::Float32, DefaultRule::Zero, 0x38},
            {Field::YOffset, WireKind::Float32, DefaultRule::Zero, 0x3C},
            {Field::Pitch, WireKind::Float32, DefaultRule::Zero, 0x40},
        }};
    }

    std::vector<spColorFuncEvalSerializer::Field>
    spColorFuncEvalSerializer::BuildWritePlanForAnalysis(
        const WriteShape& shape,
        const std::uint32_t platformDefaultColorARGB)
    {
        std::vector<Field> plan;
        if (shape.color1ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color1);
        }
        if (shape.color2ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color2);
        }
        if (shape.functionType != 0)
        {
            plan.push_back(Field::FunctionType);
        }
        if (shape.frequency != 1.0F)
        {
            plan.push_back(Field::Frequency);
        }
        if (shape.amplitude != 1.0F)
        {
            plan.push_back(Field::Amplitude);
        }
        if (shape.xOffset != 0.0F)
        {
            plan.push_back(Field::XOffset);
        }
        if (shape.yOffset != 0.0F)
        {
            plan.push_back(Field::YOffset);
        }
        if (shape.pitch != 0.0F)
        {
            plan.push_back(Field::Pitch);
        }
        return plan;
    }

    spColorFuncEvalSerializer::FrequencyState
    spColorFuncEvalSerializer::DecodeFrequencyForAnalysis(
        const float wireFrequency) noexcept
    {
        return {wireFrequency, 1.0F / wireFrequency};
    }

    bool spColorFuncEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Pitch);
    }
}
