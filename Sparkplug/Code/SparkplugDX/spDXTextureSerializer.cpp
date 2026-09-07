#include "spDXTextureSerializer.h"
#include "spDXTexture.h"
#include "../SparkBase/spStream.h"
#include <algorithm>
#include <utility>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXTextureSerializer>();}
        const spRTTIRecord Record{spDXTextureSerializer::ClassID,spSerializer::ClassID,"spDXTextureSerializer",&spSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spDXTextureSerializer::spDXTextureSerializer() noexcept{(void)spDXTexture::StaticRTTI();}
    const spRTTIRecord& spDXTextureSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXTextureSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spDXTextureSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXTextureSerializer>();manager.RegisterClone(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spDXTextureSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return object.IsExactly(spDXTexture::ClassID);}
    bool spDXTextureSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& stream,
        std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* text){context.failed=true;if(error)*error=text;return false;};
        auto* target=dynamic_cast<spDXTexture*>(&object);
        if(context.failed||!target||!object.IsExactly(spDXTexture::ClassID)||byteCount>16u*1024u*1024u)return fail("Invalid or oversized runtime DX texture target");
        auto remaining=byteCount;
        const auto read=[&](void* destination,std::uint32_t count){if(count>remaining||!stream.ReadData(destination,count))return false;remaining-=count;return true;};
        std::uint32_t width=0,height=0,format=0,count=0;std::uint8_t palette=0;
        if(!read(&width,4)||!read(&height,4)||!read(&format,4)||!read(&palette,1))return fail("Truncated flat DX texture header");
        // Native exactly1 means1024 palette bytes; other bytes mean absent.
        // CPU shadow does not fabricate renderer registration/index. RAII is
        // host safety: original errors/dtor may leave this allocation alive.
        std::unique_ptr<spPalette> paletteObject;
        if(palette==1)
        {
            std::array<std::byte,spPalette::EntryByteCount> entries;
            if(!read(entries.data(),static_cast<std::uint32_t>(entries.size())))return fail("Truncated runtime texture palette");
            paletteObject=std::make_unique<spPalette>();
            if(!paletteObject->SetEntriesForAnalysis(entries.data(),entries.size()))return fail("Invalid runtime palette extent");
        }
        if(!width||!height||width>65535||height>65535||(width&(width-1))||(height&(height-1))||format>=8
            ||!read(&count,4)||count!=spDXTexture::FullMipCountForAnalysis(width,height))return fail("Only complete power-of-two runtime mip chains are restored");
        std::vector<spDXTexture::MipForAnalysis> mips;auto w=width,h=height;
        for(std::uint32_t i=0;i<count;++i)
        {
            spDXTexture::MipForAnalysis mip;
            if(!spDXTexture::DescribeMipForAnalysis(w,h,format,mip))return fail("Invalid mip layout");
            const auto size=std::uint64_t(mip.rowBytes)*mip.rows;
            if(size>remaining)return fail("Truncated packed runtime mip bytes");
            // Explicit CPU storage pitch policy, never a claim about a live device.
            if(context.pcTexturePitchForAnalysis)mip.physicalPitch=context.pcTexturePitchForAnalysis(context.pcTexturePitchContext,i,mip.rowBytes);
            if(mip.physicalPitch<mip.rowBytes||std::uint64_t(mip.physicalPitch)*mip.rows>16u*1024u*1024u)return fail("Invalid declared texture pitch");
            mip.packedBytes.resize(static_cast<std::size_t>(size));
            if(!read(mip.packedBytes.data(),static_cast<std::uint32_t>(size)))return fail("Cannot read packed runtime mip bytes");
            mips.push_back(std::move(mip));w=std::max(1u,w>>1);h=std::max(1u,h>>1);
        }
        if(remaining||!target->InitializeRuntimeMipShadowForAnalysis(width,height,format,std::move(mips)))return fail("Invalid runtime texture extent/state");
        target->AdoptPaletteForAnalysis(std::move(paletteObject));
        return true;
    }
    bool spDXTextureSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* text){if(error)*error=text;return false;};
        const auto* target=dynamic_cast<const spDXTexture*>(&object);
        if(!target||!object.IsExactly(spDXTexture::ClassID)||!target->IsInitializedForAnalysis()
            ||!target->HasInitializedRuntimeFormatForAnalysis()||target->GetMipsForAnalysis().empty()
            ||target->GetRuntimeFormatForAnalysis()!=target->GetSurfaceFormatForAnalysis())return fail("Uninitialized or stale runtime texture format/mips");
        const auto* paletteObject=target->GetPaletteForAnalysis();
        if(paletteObject&&!paletteObject->HasInitializedEntriesForAnalysis())return fail("Uninitialized runtime palette entries");
        const std::uint32_t header[]{target->GetWidthForAnalysis(),target->GetHeightForAnalysis(),target->GetRuntimeFormatForAnalysis()};
        const std::uint8_t palette=paletteObject?1:0;const auto count=static_cast<std::uint32_t>(target->GetMipsForAnalysis().size());
        if(!stream.WriteData(header,sizeof(header))||!stream.WriteData(&palette,1))return fail("Cannot write runtime texture header");
        if(paletteObject&&!stream.WriteData(paletteObject->GetEntriesForAnalysis().data(),spPalette::EntryByteCount))return fail("Cannot write runtime palette");
        if(!stream.WriteData(&count,4))return fail("Cannot write runtime mip count");
        for(const auto& mip:target->GetMipsForAnalysis())if(!stream.WriteData(mip.packedBytes.data(),static_cast<std::uint32_t>(mip.packedBytes.size())))return fail("Cannot write packed runtime mip bytes");
        return true;
    }
}
