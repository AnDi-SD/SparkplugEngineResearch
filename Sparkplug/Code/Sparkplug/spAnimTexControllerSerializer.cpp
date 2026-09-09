#include "spAnimTexControllerSerializer.h"
#include "spAnimTexController.h"
#include "spTexture.h"
#include "spSerializerManager.h"
#include "spResourceManager.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateAnimTexControllerSerializer()
        {
            return std::make_unique<spAnimTexControllerSerializer>();
        }

        const spRTTIRecord AnimTexControllerSerializerRecord{
            spAnimTexControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spAnimTexControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateAnimTexControllerSerializer,
            nullptr,
        };

        const bool AnimTexControllerSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(AnimTexControllerSerializerRecord);
    }

    bool spAnimTexControllerSerializer::SegmentPlan::operator==(
        const SegmentPlan& other) const noexcept
    {
        return segment == other.segment
            && elementCount == other.elementCount
            && fixedByteCount == other.fixedByteCount
            && variableLength == other.variableLength;
    }

    spAnimTexControllerSerializer::~spAnimTexControllerSerializer() = default;

    spAnimTexControllerSerializer::spAnimTexControllerSerializer() noexcept
    {(void)spAnimTexController::StaticRTTI();}

    const spRTTIRecord& spAnimTexControllerSerializer::StaticRTTI() noexcept
    {
        (void)AnimTexControllerSerializerRegistered;
        return AnimTexControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spAnimTexControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spAnimTexControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spAnimTexControllerSerializer::vfunc_18() const noexcept
    {
        return AnimTexControllerSerializerRecord;
    }

    spClassID
    spAnimTexControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spAnimTexControllerSerializer::Field>
    spAnimTexControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::ControllerBase};
    }

    spAnimTexControllerSerializer::PayloadPlan
    spAnimTexControllerSerializer::BuildPayloadPlanForAnalysis(
        const std::uint32_t frameCount) noexcept
    {
        return {{
            {PayloadSegment::FrameCount, 1, sizeof(std::uint32_t), false},
            {PayloadSegment::TimeArray, frameCount,
                static_cast<std::uint64_t>(frameCount) * sizeof(float), false},
            {PayloadSegment::TextureRelationships, frameCount, 0, true},
        }};
    }

    bool spAnimTexControllerSerializer::HasConsistentTrackShapeForAnalysis(
        const std::uint32_t timeCount,
        const std::uint32_t textureRelationshipCount) noexcept
    {
        return timeCount == textureRelationshipCount;
    }

    bool spAnimTexControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::ControllerBase);
    }

    bool spAnimTexControllerSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {return ReadFieldsForAnalysis(context,source,byteCount,object,error,nullptr);}

    bool spAnimTexControllerSerializer::InspectPayloadForAnalysis(spStream& source,std::uint32_t byteCount,
        spAnimTexController& partial,InspectionForAnalysis& observation,std::string* error) const
    {
        if(error)error->clear();observation={};spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);
        return ReadFieldsForAnalysis(context,source,byteCount,partial,error,&observation);
    }

    bool spAnimTexControllerSerializer::ReadFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error,InspectionForAnalysis* observation) const
    {
        evidence::pc::serialization::SectionCursor cursor(context,source,byteCount,true,error);
        auto* controller=dynamic_cast<spAnimTexController*>(&object);
        if(!controller)return cursor.Fail("Animated texture target must be spAnimTexController");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip animated texture field");continue;}
            std::uint32_t count=0;
            if(header->payloadSize<4||!source.Read(count)||count>spTextureTrack::MaximumKeysForAnalysis
                ||std::uint64_t(count)*8+4>header->payloadSize)return cursor.Fail("Animated texture count exceeds bounded field");
            std::vector<float> times(count);std::vector<std::shared_ptr<spTexture>> textures; textures.reserve(count);
            std::vector<evidence::pc::serialization::InspectedReference> inspected;
            if(observation)inspected.reserve(count);
            // Native calls ReadData even for count0: FileStream's zero-byte
            // read fails, MemoryStream may succeed. Keep the stream contract.
            if(!source.ReadData(times.data(),count*4))return cursor.Fail("Cannot read animated texture times");
            for(std::uint32_t i=0;i<count;++i)
            {
                if(observation)
                {
                    const auto end=header->dataStreamPosition+header->payloadSize;std::uint32_t position=0;
                    evidence::pc::serialization::InspectedReference reference;
                    if(!source.GetCurrentPosition(position)||position>end||
                        !evidence::pc::serialization::InspectReference(source,end-position,false,reference,error))
                        return cursor.Fail("Cannot inspect animated texture frame");
                    inspected.push_back(reference);textures.emplace_back();continue;
                }
                auto* referenced=spSerializer::ReadSequenceReferenceForAnalysis(context,spTexture::ClassID,source,
                    header->dataStreamPosition+header->payloadSize,error);
                if(context.failed)return false;
                auto owner=std::dynamic_pointer_cast<spTexture>(context.ShareObjectForAnalysis(referenced));
                if(referenced&&!owner)return cursor.Fail("Animated texture frame lacks canonical owner");
                textures.push_back(std::move(owner));
            }
            if(!controller->GetTextureTrackForAnalysis().SetKeysForAnalysis(std::move(times),std::move(textures)))
                return cursor.Fail("Cannot install bounded animated texture track");
            if(observation){observation->hasTrack=true;observation->textures=std::move(inspected);}
        }
        return false;
    }
    bool spAnimTexControllerSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        const auto* controller=dynamic_cast<spAnimTexController*>(&object);if(!controller)return false;
        for(const auto& texture:controller->GetTextureTrackForAnalysis().GetTexturesForAnalysis())
            if(!spSerializer::IndexReferenceForAnalysis(manager,texture.get()))return false;
        return true;
    }
    bool spAnimTexControllerSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& destination,const spBaseObject& object,std::string* error) const
    {
        const auto* controller=dynamic_cast<const spAnimTexController*>(&object);
        if(!controller){if(error)*error="Animated texture writer target mismatch";return false;}
        const auto& track=controller->GetTextureTrackForAnalysis();const auto& times=track.GetTimesForAnalysis();
        const auto count=static_cast<std::uint32_t>(times.size());spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(destination,&object)
            ||!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32)
            ||!destination.Write(count)||!destination.WriteData(times.data(),count*4))return false;
        for(const auto& texture:track.GetTexturesForAnalysis())
            if(!spSerializer::WriteReferenceForAnalysis(manager,destination,texture.get(),error))return false;
        return blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis();
    }
}
