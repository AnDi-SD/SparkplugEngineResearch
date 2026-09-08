#include "spTextureDataSerializer.h"

#include "spTextureData.h"
#include "../SparkplugDX/spDXTexture.h"
#include "spSerializerManager.h"
#include "spDataBlockSerializer.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spTextureResizeFilter.h"

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextureDataSerializer()
        {
            return std::make_unique<spTextureDataSerializer>();
        }

        const spRTTIRecord TextureDataSerializerRecord{
            spTextureDataSerializer::ClassID,
            spSerializer::ClassID,
            "spTextureDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateTextureDataSerializer,
            nullptr,
        };

        const bool TextureDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TextureDataSerializerRecord);
    }

    spTextureDataSerializer::~spTextureDataSerializer() = default;

    std::unique_ptr<spBaseObject> spTextureDataSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source,spSerializerObjectHeaderForAnalysis* observedHeader) const
    {
        spSerializerObjectHeaderForAnalysis header;
        if(!source.ReadData(&header,sizeof(header)))return nullptr;
        if(observedHeader)*observedHeader=header;
        // Actual PC42DD10 ignores both words and creates DXTexture4AB520.
        // Portable shadow preserves identity; no live COM acquisition is claimed.
        return std::make_unique<spDXTexture>();
    }

    bool spTextureDataSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        if(error)error->clear();SectionCursor wrapper(context,source,byteCount,false,error);
        auto* texture=dynamic_cast<spTextureData*>(&object);
        auto* runtime=dynamic_cast<spDXTexture*>(&object);
        if(!IsExactly(ClassID)||(!texture&&!runtime))return wrapper.Fail("Compared CPU TextureData or runtime DXTexture target required");
        std::uint32_t remaining=0;bool handled=false;
        if(!ReadSourceWrapperForAnalysis(context,source,byteCount,object,remaining,handled,error))return false;
        if(handled)return remaining==0?true:wrapper.Fail("Trailing bytes after embedded source section");
        SectionCursor local(context,source,remaining,true,error);bool initialized=false;
        while(const auto* header=local.Next())
        {
            if(header->IsTerminator())return initialized?true:local.Fail("No restored texture pixels");
            if(header->fieldID==1)return local.Fail("Native DX texture payload is not yet restored");
            if(header->fieldID!=0){if(!local.Skip())return local.Fail("Cannot skip local texture field");continue;}
            if(!ReadCrossSectionForAnalysis(context,source,header->payloadSize,
                [&](const spTextureBuffer& buffer){return texture?texture->InitializeFromTextureBufferForAnalysis(buffer,1,0,true)
                    :InitializeCrossDXForAnalysis(context,buffer,*runtime);},initialized,error))return false;
        }
        return false;
    }

    bool spTextureDataSerializer::InitializeCrossDXForAnalysis(spSerializerReadContextForAnalysis& context,
        const spTextureBuffer& buffer,spDXTexture& texture)
    {
        const auto w=buffer.GetWidthForAnalysis(),h=buffer.GetHeightForAnalysis();
        const auto format=buffer.GetPixelFormatForAnalysis();if(format>4)return false;
        const auto width=spTexture::NormalizeDimensionForAnalysis(w),height=spTexture::NormalizeDimensionForAnalysis(h);
        spDXTexture::MipForAnalysis base;
        if(!spDXTexture::DescribeMipForAnalysis(w,h,format+3,base))return false;
        base.packedBytes=buffer.GetBufferForAnalysis();
        if(w!=width||h!=height)
        {
            spDXTexture::MipForAnalysis resized;
            if(!sparkplug::evidence::pc::texture_mips::ResampleRaw(base,format,width,height,true,resized))return false;
            base=std::move(resized);
        }
        std::vector<spDXTexture::MipForAnalysis> mips;mips.push_back(std::move(base));
        while(mips.size()<spDXTexture::FullMipCountForAnalysis(width,height))
        {
            spDXTexture::MipForAnalysis next;
            const auto& previous=mips.back();
            const bool generated=format==0?sparkplug::evidence::pc::texture_mips::GenerateNext(previous,next)
                :sparkplug::evidence::pc::texture_mips::ResampleRaw(previous,format,std::max(1u,previous.width/2),std::max(1u,previous.height/2),false,next);
            if(!generated)return false;
            mips.push_back(std::move(next));
        }
        for(std::size_t i=0;i<mips.size();++i)if(context.pcTexturePitchForAnalysis)
            mips[i].physicalPitch=context.pcTexturePitchForAnalysis(context.pcTexturePitchContext,static_cast<std::uint32_t>(i),mips[i].rowBytes);
        return texture.InitializeCrossMipShadowForAnalysis(w,h,format,std::move(mips));
    }

    bool spTextureDataSerializer::ReadCrossSectionForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,const std::function<bool(const spTextureBuffer&)>& initialize,
        bool& initialized,std::string* error)
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        SectionCursor pixelsSection(context,source,byteCount,true,error);bool terminated=false;
        while(const auto* pixelsHeader=pixelsSection.Next())
        {
            if(pixelsHeader->IsTerminator()){terminated=true;break;}
            if(pixelsHeader->fieldID!=5){if(!pixelsSection.Skip())return pixelsSection.Fail("Cannot skip texture pixel field");continue;}
            struct Raw{std::uint32_t width,height,format,pixelSize;};Raw raw{};
            if(pixelsHeader->payloadSize<sizeof(raw)||!source.ReadData(&raw,sizeof(raw)))return pixelsSection.Fail("Truncated four-word raw texture header");
            const auto expectedSize=spTextureBuffer::PixelSizeForFormatForAnalysis(raw.format);
            const auto size=std::uint64_t(raw.width)*raw.height*raw.pixelSize;
            if(!raw.width||!raw.height||raw.width>65535||raw.height>65535||!expectedSize||raw.pixelSize!=expectedSize
                ||size>16u*1024u*1024u||size!=pixelsHeader->payloadSize-sizeof(raw))return pixelsSection.Fail("Raw pixel extent/format exceeds safe native-compatible bounds");
            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            if(!source.ReadData(bytes.data(),static_cast<std::uint32_t>(size)))return pixelsSection.Fail("Truncated raw pixels");
            spTextureBuffer buffer;
            if(!buffer.InitializeForAnalysis(static_cast<std::uint16_t>(raw.width),static_cast<std::uint16_t>(raw.height),1,raw.format)
                ||!buffer.SetDataForAnalysis(bytes)||!initialize(buffer))return pixelsSection.Fail("Cannot initialize texture from common pixel buffer");
            initialized=true;
        }
        return terminated;
    }

    bool spTextureDataSerializer::ReadSourceWrapperForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::uint32_t& remaining,bool& handled,std::string* error) const
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        SectionCursor wrapper(context,source,byteCount,false,error);std::uint32_t start=0;handled=false;remaining=0;
        if(!source.GetCurrentPosition(start))return wrapper.Fail("Cannot locate texture source section");
        while(const auto* header=wrapper.Next())
        {
            if(header->IsTerminator())
            {
                std::uint32_t position=0;
                if(!source.GetCurrentPosition(position)||position<start||position-start>byteCount)return wrapper.Fail("Invalid texture source extent");
                remaining=byteCount-(position-start);return true;
            }
            if(header->fieldID==4)return wrapper.Fail("External texture source resolver is not restored");
            if(header->fieldID==3)
            {
                if(context.depth>=64)return wrapper.Fail("Embedded texture recursion limit");
                struct DepthGuard{spSerializerReadContextForAnalysis& context;explicit DepthGuard(spSerializerReadContextForAnalysis& c):context(c){++context.depth;}~DepthGuard(){--context.depth;}} depth(context);
                const bool ok=ReadPayloadForAnalysis(context,source,header->payloadSize,object,error);
                if(!ok)return false;handled=true;
            }
            else
            {
                if(!wrapper.Skip())return wrapper.Fail("Cannot skip texture source field");
                // Actual42EC44 skips SourceNone payload, then resets handled.
                if(header->fieldID==2)handled=false;
            }
        }
        return false;
    }

    bool spTextureDataSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {spSerializerManager manager;return WritePayloadWithContextForAnalysis(manager,stream,object,error);}

    bool spTextureDataSerializer::WriteSourceNoneForAnalysis(spStream& stream,const spTextureData& texture)
    {
        spDataBlockSerializer wrapper;const std::uint8_t none=0;
        return wrapper.BeginObjectForAnalysis(stream,&texture)&&wrapper.WriteFieldForAnalysis(stream,2,&none,1)
            &&wrapper.FinalizeObjectForAnalysis();
    }
    bool spTextureDataSerializer::WriteCrossSectionForAnalysis(spStream& stream,const spTextureData& texture)
    {
        const auto header=BuildCrossPlatformPayloadHeaderForAnalysis(texture);
        if(!header.valid||!header.payloadSize||header.payloadSize>16u*1024u*1024u)return false;
        const std::uint32_t raw[]{header.width,header.height,header.pixelFormat,header.pixelSize};
        spDataBlockSerializer nested;
        return nested.BeginObjectForAnalysis(stream,&texture)&&nested.WriteBeginForAnalysis(5)
            &&stream.WriteData(raw,sizeof(raw))&&stream.WriteData(texture.GetTextureBufferForAnalysis().GetBufferForAnalysis().data(),header.payloadSize)
            &&nested.WriteEndForAnalysis(5)&&nested.FinalizeObjectForAnalysis();
    }

    bool spTextureDataSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* text){if(error)*error=text;return false;};
        const auto* texture=dynamic_cast<const spTextureData*>(&object);
        if(!IsExactly(ClassID)||!texture||!object.IsExactly(spTextureData::ClassID))return fail("Only verified CPU TextureData writer is restored");
        const auto header=BuildCrossPlatformPayloadHeaderForAnalysis(*texture);
        if(!header.valid||!header.payloadSize||header.payloadSize>16u*1024u*1024u)return fail("Uninitialized or oversized CPU texture");
        if(!WriteSourceNoneForAnalysis(stream,*texture))return fail("Cannot write texture source wrapper");
        spDataBlockSerializer local;if(!local.BeginObjectForAnalysis(stream,texture))return fail("Cannot begin local texture section");
        const auto policy=manager.GetSerializationPolicyForAnalysis();
        if(policy==0||policy==2)
        {
            const std::uint32_t platform=1;
            if(!local.WriteBeginForAnalysis(6)||!stream.WriteData(&platform,4)||!local.WriteEndForAnalysis(6)
                ||!local.WriteBeginForAnalysis(0)||!WriteCrossSectionForAnalysis(stream,*texture)
                ||!local.WriteEndForAnalysis(0))return fail("Cannot write nested raw CPU texture fields");
        }
        return local.FinalizeObjectForAnalysis()?true:fail("Cannot finalize local texture section");
    }

    bool spTextureDataSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return object.IsExactly(spTextureData::ClassID);}

    const spRTTIRecord& spTextureDataSerializer::StaticRTTI() noexcept
    {
        (void)TextureDataSerializerRegistered;
        return TextureDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTextureDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTextureDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTextureDataSerializer::vfunc_18() const noexcept
    {
        return TextureDataSerializerRecord;
    }

    spClassID spTextureDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spTextureData::ClassID;
    }

    std::vector<spTextureDataSerializer::Field>
    spTextureDataSerializer::BuildKnownWritePlanForAnalysis(
        const DataSourceKind sourceKind,
        const std::uint32_t nativeSerializationMode) const
    {
        switch (sourceKind)
        {
        case DataSourceKind::EmbeddedMemoryStream:
            return {Field::SourceEmbeded};
        case DataSourceKind::ReferencedStream:
            return {Field::SourceReference};
        case DataSourceKind::None:
            break;
        }

        std::vector<Field> plan{Field::SourceNone};
        if (nativeSerializationMode == 0 || nativeSerializationMode == 2)
        {
            plan.push_back(Field::PlatformType);
            plan.push_back(Field::CrossPlatform);
        }
        return plan;
    }

    spTextureDataSerializer::CrossPlatformPayloadHeader
    spTextureDataSerializer::BuildCrossPlatformPayloadHeaderForAnalysis(
        const spTextureData& textureData) noexcept
    {
        CrossPlatformPayloadHeader result;
        const auto& buffer = textureData.GetTextureBufferForAnalysis();
        if (!buffer.IsInitializedForAnalysis())
        {
            return result;
        }

        result.width = buffer.GetWidthForAnalysis();
        result.height = buffer.GetHeightForAnalysis();
        result.pixelFormat = buffer.GetPixelFormatForAnalysis();
        result.pixelSize = buffer.GetPixelSizeForAnalysis();

        const std::uint64_t payloadSize =
            static_cast<std::uint64_t>(result.width)
            * result.height
            * result.pixelSize;
        if (payloadSize > std::numeric_limits<std::uint32_t>::max()
            || payloadSize > buffer.GetBufferForAnalysis().size())
        {
            return result;
        }

        result.payloadSize = static_cast<std::uint32_t>(payloadSize);
        result.valid = true;
        return result;
    }
}
