#include "spSkinSerializer.h"

#include "spNode.h"
#include "spSkin.h"
#include "Analysis/PC/spSectionCursor.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSkinSerializer()
        {
            return std::make_unique<spSkinSerializer>();
        }

        const spRTTIRecord SkinSerializerRecord{
            spSkinSerializer::ClassID,
            spModelSerializer::ClassID,
            "spSkinSerializer",
            &spModelSerializer::StaticRTTI(),
            &CreateSkinSerializer,
            nullptr,
        };

        const bool SkinSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SkinSerializerRecord);
    }

    bool spSkinSerializer::PayloadPlanEntry::operator==(
        const PayloadPlanEntry& other) const noexcept
    {
        return segment == other.segment
            && elementCount == other.elementCount
            && byteCount == other.byteCount;
    }

    bool spSkinSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return model == other.model
            && skinFields == other.skinFields
            && payload == other.payload;
    }

    spSkinSerializer::~spSkinSerializer() = default;

    const spRTTIRecord& spSkinSerializer::StaticRTTI() noexcept
    {
        (void)SkinSerializerRegistered;
        return SkinSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSkinSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSkinSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spSkinSerializer::vfunc_18() const noexcept
    {
        return SkinSerializerRecord;
    }

    spClassID spSkinSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spSkin::ClassID;
    }

    bool spSkinSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* skin=dynamic_cast<spSkin*>(&object);
        if(!skin){context.failed=true;if(error)*error="Skin reader requires Skin target";return false;}
        std::uint32_t start=0,position=0;
        if(!stream.GetCurrentPosition(start)||!ReadModelFieldsForAnalysis(context,stream,size,*skin,false,error)
            ||!stream.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing Skin section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,stream,size-(position-start),true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID!=0)
            {if(!cursor.Skip())return cursor.Fail("Cannot skip Skin field");continue;}
            std::uint32_t weights=0,count=0;
            if(header->payloadSize<8||!stream.ReadData(&weights,4)||!stream.ReadData(&count,4))
                return cursor.Fail("Invalid Skin weight/bone counts");
            // At least a four-byte reference and 64 matrix bytes per bone.
            // This host envelope guard precedes allocation and is stricter
            // than native491170's unchecked count arithmetic.
            if(count>(header->payloadSize-8)/68)return cursor.Fail("Skin bones exceed field extent");
            const auto end=header->dataStreamPosition+header->payloadSize;
            std::vector<spSkin::BoneBinding> bindings;bindings.reserve(count);
            for(std::uint32_t i=0;i<count;++i)
            {
                auto* raw=ReadSequenceReferenceForAnalysis(context,BoneRelationshipClassID,stream,end,error);
                if(context.failed)return false;
                spSkin::Matrix4 matrix{};
                if(!stream.GetCurrentPosition(position)||position>end||end-position<sizeof(matrix)
                    ||!stream.ReadData(matrix.data(),sizeof(matrix)))
                    return cursor.Fail("Invalid Skin inverse-bind matrix");
                // Original consumes the matrix before rejecting a null bone.
                // Its borrowed pointer is kept alive by explicit host ownership.
                auto bone=std::dynamic_pointer_cast<spNode>(context.ShareObjectForAnalysis(raw));
                if(!bone)return cursor.Fail("Skin bone is null, wrong type or lacks an explicit owner");
                bindings.push_back(spSkin::BoneBinding::BorrowedForAnalysis(bone,matrix));
            }
            // Original replaces both arrays without freeing old ones. The
            // portable container deliberately releases replaced storage.
            if(!skin->SetPaletteForAnalysis(weights,std::move(bindings)))return cursor.Fail("Invalid Skin palette");
        }
        return false;
    }

    bool spSkinSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
        spBaseObject& object) const
    {
        auto* skin=dynamic_cast<spSkin*>(&object);
        if(!skin||!IndexModelFieldsForAnalysis(manager,*skin))return false;
        for(const auto& binding:skin->GetBoneBindingsForAnalysis())
        {
            const auto bone=binding.GetBoneForAnalysis();
            if(!bone||!IndexReferenceForAnalysis(manager,bone.get()))return false;
        }
        return true;
    }

    bool spSkinSerializer::WriteSectionsForAnalysis(spSerializerManager* manager,spStream& stream,
        const spSkin& skin,std::string* error) const
    {
        if(error)error->clear();
        if(!manager&&skin.GetBoneCountForAnalysis())
        {if(error)*error="Skin graph writer requires explicit manager";return false;}
        if(!WriteModelFieldsForAnalysis(manager,stream,skin,error))return false;
        spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,&skin)
            ||!blocks.WriteBeginForAnalysis(0,spDataBlockSerializer::SizeCode::UInt32))return false;
        const auto weights=skin.GetWeightCountForAnalysis();
        const auto count=static_cast<std::uint32_t>(skin.GetBoneCountForAnalysis());
        if(!stream.WriteData(&weights,4)||!stream.WriteData(&count,4))return false;
        for(const auto& binding:skin.GetBoneBindingsForAnalysis())
        {
            const auto bone=binding.GetBoneForAnalysis();
            if(!bone||!WriteReferenceForAnalysis(*manager,stream,bone.get(),error)
                ||!stream.WriteData(binding.inverseBindMatrix.data(),sizeof(binding.inverseBindMatrix)))return false;
        }
        return blocks.WriteEndForAnalysis(0)&&blocks.FinalizeObjectForAnalysis();
    }

    bool spSkinSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* skin=dynamic_cast<const spSkin*>(&object);
        if(!skin){if(error)*error="Skin writer requires Skin target";return false;}
        return WriteSectionsForAnalysis(nullptr,stream,*skin,error);
    }

    bool spSkinSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        const auto* skin=dynamic_cast<const spSkin*>(&object);
        if(!skin){if(error)*error="Skin writer requires Skin target";return false;}
        return WriteSectionsForAnalysis(&manager,stream,*skin,error);
    }

    spSkinSerializer::KnownWritePlan
    spSkinSerializer::BuildKnownWritePlanForAnalysis(const spSkin& skin) const
    {
        const auto boneCount = skin.GetBoneCountForAnalysis();
        return {
            spModelSerializer::BuildKnownWritePlanForAnalysis(skin),
            {Field::Skin},
            {
                {PayloadSegment::WeightCount, 1, sizeof(std::uint32_t)},
                {PayloadSegment::BoneCount, 1, sizeof(std::uint32_t)},
                {PayloadSegment::BoneRelationship, boneCount, 0},
                {PayloadSegment::InverseBindMatrix, boneCount,
                    boneCount * sizeof(spSkin::Matrix4)},
            },
        };
    }

    bool spSkinSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::Skin);
    }
}
