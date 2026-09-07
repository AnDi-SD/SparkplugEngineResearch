#include "spFunctionEvalSerializer.h"
#include "spFunctionEval.h"
#include "Analysis/PC/spSectionCursor.h"
#include <cstring>

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFunctionEvalSerializer()
        {
            return std::make_unique<spFunctionEvalSerializer>();
        }

        const spRTTIRecord FunctionEvalSerializerRecord{
            spFunctionEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spFunctionEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateFunctionEvalSerializer,
            nullptr,
        };

        const bool FunctionEvalSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(FunctionEvalSerializerRecord);
    }

    bool spFunctionEvalSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireKind == other.wireKind
            && defaultRule == other.defaultRule
            && targetOffset == other.targetOffset;
    }

    spFunctionEvalSerializer::spFunctionEvalSerializer() noexcept{(void)spFunctionEval::StaticRTTI();}
    spFunctionEvalSerializer::~spFunctionEvalSerializer() = default;

    bool spFunctionEvalSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {return ReadFunctionFieldsForAnalysis(context,stream,size,object,true,error);}
    bool spFunctionEvalSerializer::ReadFunctionFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,bool requireExactEnd,std::string* error) const
    {
        if(error)error->clear();auto* evaluator=dynamic_cast<spFunctionEval*>(&object);
        if(!evaluator){context.failed=true;if(error)*error="FunctionEval target mismatch";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,requireExactEnd,error);
        while(const auto* field=cursor.Next())
        {
            if(field->IsTerminator())return true;
            if(field->fieldID>5){if(!cursor.Skip())return cursor.Fail("Cannot skip FunctionEval field");continue;}
            std::uint32_t raw=0;if(!cursor.Read(raw))return cursor.Fail("FunctionEval scalar requires four bytes");
            if(!ApplyRawStateFieldForAnalysis(*evaluator,field->fieldID,raw))return cursor.Fail("Unknown scalar state field");
        }
        return false;
    }
    bool spFunctionEvalSerializer::ApplyRawStateFieldForAnalysis(spFunctionEval& evaluator,std::uint32_t id,std::uint32_t raw) noexcept
    {
        auto state=evaluator.GetStateForAnalysis();float value=0;std::memcpy(&value,&raw,4);
        switch(id)
        {
        case 0:state.functionType=raw;break;
        case 1:state.frequency=value;state.reciprocal=1.0F/value;break;
        case 2:state.amplitude=value;break;
        case 3:state.xOffset=value;break;
        case 4:state.yOffset=value;break;
        case 5:state.pitch=value;break;
        default:return false;
        }
        evaluator.SetStateForAnalysis(state);return true;
    }
    bool spFunctionEvalSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* evaluator=dynamic_cast<const spFunctionEval*>(&object);
        if(!evaluator){if(error)*error="FunctionEval writer target mismatch";return false;}
        const auto& state=evaluator->GetStateForAnalysis();
        const auto plan=BuildWritePlanForAnalysis({state.functionType,state.frequency,state.amplitude,state.xOffset,state.yOffset,state.pitch});
        const float values[]{state.frequency,state.amplitude,state.xOffset,state.yOffset,state.pitch};
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,evaluator))return false;
        for(const auto field:plan)
        {
            const auto id=static_cast<std::uint32_t>(field);
            const void* data=id?static_cast<const void*>(&values[id-1]):static_cast<const void*>(&state.functionType);
            if(!blocks.WriteFieldForAnalysis(stream,id,data,4))
            {if(error)*error="Cannot write FunctionEval scalar";return false;}
        }
        if(blocks.FinalizeObjectForAnalysis())return true;
        if(error)*error="Cannot finish FunctionEval section";return false;
    }
    bool spFunctionEvalSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return dynamic_cast<spFunctionEval*>(&object)!=nullptr;}

    const spRTTIRecord& spFunctionEvalSerializer::StaticRTTI() noexcept
    {
        (void)FunctionEvalSerializerRegistered;
        return FunctionEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spFunctionEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spFunctionEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spFunctionEvalSerializer::vfunc_18() const noexcept
    {
        return FunctionEvalSerializerRecord;
    }

    spClassID spFunctionEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spFunctionEvalSerializer::FieldSchema
    spFunctionEvalSerializer::BuildFieldSchemaForAnalysis() noexcept
    {
        return {{
            {Field::FunctionType, WireKind::UInt32, DefaultRule::Zero, 0x34},
            {Field::Frequency, WireKind::Float32, DefaultRule::One, 0x14},
            {Field::Amplitude, WireKind::Float32, DefaultRule::One, 0x1C},
            {Field::XOffset, WireKind::Float32, DefaultRule::Zero, 0x20},
            {Field::YOffset, WireKind::Float32, DefaultRule::Zero, 0x24},
            {Field::Pitch, WireKind::Float32, DefaultRule::Zero, 0x28},
        }};
    }

    std::vector<spFunctionEvalSerializer::Field>
    spFunctionEvalSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
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

    spFunctionEvalSerializer::FrequencyState
    spFunctionEvalSerializer::DecodeFrequencyForAnalysis(
        const float wireFrequency) noexcept
    {
        return {wireFrequency, 1.0F / wireFrequency};
    }

    bool spFunctionEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Pitch);
    }
}
