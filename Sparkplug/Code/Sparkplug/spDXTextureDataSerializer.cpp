#include "spDXTextureDataSerializer.h"
#include "../SparkplugDX/spDXTexture.h"
#include "spSerializerManager.h"
#include "spTextureData.h"
#include "spDataBlockSerializer.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spTextureMipFilter.h"
#include "Analysis/PC/spTextureResizeFilter.h"
#include "Analysis/PC/spTextureCompressedMipFilter.h"
#include <algorithm>

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXTextureDataSerializer()
        {
            return std::make_unique<spDXTextureDataSerializer>();
        }

        const spRTTIRecord DXTextureDataSerializerRecord{
            spDXTextureDataSerializer::ClassID,
            spTextureDataSerializer::ClassID,
            "spDXTextureDataSerializer",
            &spTextureDataSerializer::StaticRTTI(),
            &CreateDXTextureDataSerializer,
            nullptr,
        };

        const bool DXTextureDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXTextureDataSerializerRecord);
    }

    spDXTextureDataSerializer::~spDXTextureDataSerializer() = default;

    bool spDXTextureDataSerializer::WriteMipRecordForAnalysis(spStream& stream,const spTextureData& texture,
        const spTextureData::NativeMipForAnalysis& mip,bool first)
    {
        if(first)
        {
            const std::uint8_t nativeFlag=1,field1C=texture.GetField1CForAnalysis();
            const std::uint32_t header[]{texture.GetWidthForAnalysis(),texture.GetHeightForAnalysis(),texture.GetTextureFlagsForAnalysis()};
            if(!stream.WriteData(&nativeFlag,1)||!stream.WriteData(header,sizeof(header))||!stream.WriteData(&field1C,1))return false;
        }
        const std::uint32_t rowHeader[]{mip.width,mip.rowStride,mip.rows};
        return stream.WriteData(rowHeader,sizeof(rowHeader))&&stream.WriteData(mip.bytes.data(),static_cast<std::uint32_t>(mip.bytes.size()));
    }

    bool spDXTextureDataSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* text){if(error)*error=text;return false;};
        const auto* texture=dynamic_cast<const spTextureData*>(&object);
        if(!texture||!object.IsExactly(spTextureData::ClassID)||!texture->GetField68ForAnalysis()||texture->GetNativeMipsForAnalysis().empty())
            return fail("Native data writer requires initialized CPU TextureData mip storage, not runtime DXTexture");
        const auto policy=manager.GetSerializationPolicyForAnalysis();const bool cross=policy==0||policy==2;
        const auto crossHeader=BuildCrossPlatformPayloadHeaderForAnalysis(*texture);
        if(cross&&(!crossHeader.valid||!crossHeader.payloadSize||crossHeader.payloadSize>16u*1024u*1024u))return fail("Cross+native writer requires separately initialized CPU pixel buffer");
        if(texture->GetTextureFlagsForAnalysis()>3)return fail("Unsupported native mip flags");
        // Revalidate against mutable base state before any output. The native
        // class exposes independent setters; host must not emit stale layouts.
        spTextureData validation;
        if(!validation.SetNativeMipDataForAnalysis(texture->GetWidthForAnalysis(),texture->GetHeightForAnalysis(),
            texture->GetTextureFlagsForAnalysis(),texture->GetField1CForAnalysis(),texture->GetNativeMipsForAnalysis()))return fail("Stale native mip layout");
        if(!WriteSourceNoneForAnalysis(stream,*texture))return fail("Cannot write source wrapper");
        spDataBlockSerializer local;if(!local.BeginObjectForAnalysis(stream,texture))return fail("Cannot begin local DX texture section");
        const std::uint32_t platform=cross?7u:6u;
        if(!local.WriteBeginForAnalysis(6)||!stream.WriteData(&platform,4)||!local.WriteEndForAnalysis(6))return fail("Cannot write DX platform");
        if(cross&&(!local.WriteBeginForAnalysis(0)||!WriteCrossSectionForAnalysis(stream,*texture)||!local.WriteEndForAnalysis(0)))return fail("Cannot write shared cross section");
        if(!local.WriteBeginForAnalysis(1))return fail("Cannot begin native mip section");
        spDataBlockSerializer native;if(!native.BeginObjectForAnalysis(stream,texture))return fail("Cannot bind native mip writer");
        bool first=true;
        for(const auto& mip:texture->GetNativeMipsForAnalysis())
        {
            const auto field=first?0u:1u;if(!native.WriteBeginForAnalysis(field))return fail("Cannot begin mip record");
            if(!WriteMipRecordForAnalysis(stream,*texture,mip,first)
                ||!native.WriteEndForAnalysis(field))return fail("Cannot write native mip rows");
            first=false;
        }
        return native.FinalizeObjectForAnalysis()&&local.WriteEndForAnalysis(1)&&local.FinalizeObjectForAnalysis()?true:fail("Cannot finalize native texture sections");
    }

    bool spDXTextureDataSerializer::ReadNativeSectionForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t byteCount,NativeReadForAnalysis& output,std::string* error)
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        SectionCursor native(context,stream,byteCount,true,error);
        output={};
        bool prefix=false,terminated=false;
        auto& width=output.width;auto& height=output.height;auto& flags=output.flags;
        auto& nativeFlag=output.nativeFlag;auto& field1C=output.field1C;
        auto& mips=output.mips;
        while(const auto* mipField=native.Next())
        {
            if(mipField->IsTerminator()){terminated=true;break;}
            if(mipField->fieldID!=0&&mipField->fieldID!=1){if(!native.Skip())return native.Fail("Cannot skip native mip field");continue;}
            auto payload=mipField->payloadSize;
            if(mipField->fieldID==0)
            {
                if(prefix||payload<26)return native.Fail("Missing or repeated native mip prefix");
                if(!stream.ReadData(&nativeFlag,1)||!stream.ReadData(&width,4)||!stream.ReadData(&height,4)
                    ||!stream.ReadData(&flags,4)||!stream.ReadData(&field1C,1))return native.Fail("Truncated native mip prefix");
                payload-=14;prefix=true;
                if(!nativeFlag||!width||!height||width>65535||height>65535||flags>3
                    ||(flags&&((width&(width-1))||(height&(height-1)))))
                    return native.Fail("Native BGRA or power-of-two compressed mip data with confirmed flags is required");
            }
            if(!prefix||payload<12||mips.size()>=spDXTexture::FullMipCountForAnalysis(width,height))return native.Fail("Native mip count/prefix exceeds full chain");
            std::uint32_t wireWidth=0,wireStride=0,wireRows=0;
            if(!stream.ReadData(&wireWidth,4)||!stream.ReadData(&wireStride,4)||!stream.ReadData(&wireRows,4))return native.Fail("Truncated native mip row header");
            auto w=width,h=height;for(std::size_t i=0;i<mips.size();++i){w=std::max(1u,w>>1);h=std::max(1u,h>>1);}
            spDXTexture::MipForAnalysis mip;
            if(!spDXTexture::DescribeMipForAnalysis(w,h,flags?flags-1:3u,mip)||wireWidth!=w||wireStride!=mip.rowBytes||wireRows!=mip.rows
                ||std::uint64_t(wireStride)*wireRows!=payload-12)return native.Fail("Native mip layout/extent differs from confirmed packed surface");
            if(context.pcTexturePitchForAnalysis)mip.physicalPitch=context.pcTexturePitchForAnalysis(context.pcTexturePitchContext,static_cast<std::uint32_t>(mips.size()),mip.rowBytes);
            if(mip.physicalPitch<mip.rowBytes||std::uint64_t(mip.physicalPitch)*mip.rows>16u*1024u*1024u)return native.Fail("Invalid declared native mip surface pitch");
            mip.packedBytes.resize(static_cast<std::size_t>(wireStride)*wireRows);
            if(!stream.ReadData(mip.packedBytes.data(),static_cast<std::uint32_t>(mip.packedBytes.size())))return native.Fail("Truncated native mip row bytes");
            output.pixelOffsets.push_back(mipField->dataStreamPosition+(mipField->fieldID==0?26:12));
            mips.push_back(std::move(mip));
        }
        if(!terminated||!prefix||mips.empty())return native.Fail("Incomplete native mip section");
        return true;
    }

    bool spDXTextureDataSerializer::InspectPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t byteCount,spDXTexture& texture,
        spTextureReadInspectionForAnalysis& inspection,std::string* error) const
    {
        if(error)error->clear();
        if(context.textureReadInspectionForAnalysis==&inspection)
        {if(error)*error="Cannot reuse an active texture inspection";return false;}
        inspection={};
        const auto initialDepth=context.depth;
        if(!stream.GetCurrentPosition(inspection.inputOffset))
        {if(error)*error="Cannot locate texture inspection input";return false;}
        inspection.inputSize=byteCount;inspection.inputStream_=&stream;
        struct Restore final
        {
            spSerializerReadContextForAnalysis& context;
            spTextureReadInspectionForAnalysis* previous;
            const spStream*& input;
            ~Restore(){context.textureReadInspectionForAnalysis=previous;input=nullptr;}
        } restore{context,context.textureReadInspectionForAnalysis,inspection.inputStream_};
        context.textureReadInspectionForAnalysis=&inspection;
        const bool read=ReadPayloadForAnalysis(context,stream,byteCount,texture,error);
        const bool positionKnown=stream.GetCurrentPosition(inspection.finalPosition);
        inspection.complete=read&&!context.failed&&!inspection.truncated&&positionKnown&&
            std::uint64_t(inspection.inputOffset)+byteCount==inspection.finalPosition&&context.depth==initialDepth;
        if(!inspection.complete&&read&&error)
            *error=inspection.truncated?"Texture inspection observation limit reached":
                "Texture inspection ended with an incomplete cursor or recursion scope";
        return inspection.complete;
    }

    bool spDXTextureDataSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        if(error)error->clear();SectionCursor guard(context,stream,byteCount,false,error);
        auto* texture=dynamic_cast<spDXTexture*>(&object);
        if(!texture||!object.IsExactly(spDXTexture::ClassID))return guard.Fail("DX native data requires actual runtime DXTexture target");
        auto* const inspection=context.textureReadInspectionForAnalysis;
        std::uint32_t frameStart=0;
        if(inspection&&!stream.GetCurrentPosition(frameStart))return guard.Fail("Cannot locate observed DX texture frame");
        bool handled=false;std::uint32_t remaining=0;
        if(!ReadSourceWrapperForAnalysis(context,stream,byteCount,object,remaining,handled,error))return false;
        if(handled)return remaining==0?true:guard.Fail("Trailing bytes after embedded DX source");
        SectionCursor local(context,stream,remaining,true,error);auto platform=context.manager.GetPlatformMaskForAnalysis();bool initialized=false;
        while(const auto* field=local.Next())
        {
            const auto observed=inspection?inspection->BeginFieldForAnalysis(stream,*field,
                spTextureReadInspectionForAnalysis::Scope::Derived,frameStart,byteCount,context.depth,platform,handled):
                spTextureReadInspectionForAnalysis::NoField;
            const auto finish=[&](spTextureReadInspectionForAnalysis::Outcome outcome)
            {if(inspection)inspection->FinishFieldForAnalysis(observed,outcome,platform,handled);};
            if(field->IsTerminator())
            {finish(spTextureReadInspectionForAnalysis::Outcome::Terminator);return initialized?true:local.Fail("No restored native DX mip payload");}
            if(field->fieldID==6)
            {
                if(!local.Read(platform))return local.Fail("Invalid DX platform field");
                finish(spTextureReadInspectionForAnalysis::Outcome::Platform);continue;
            }
            if(field->fieldID==0&&!(platform&PCNativeLoadFlagMask))
            {
                CrossReadObserverForAnalysis observeCross;
                if(inspection&&observed!=spTextureReadInspectionForAnalysis::NoField)
                    observeCross=[&](const spTextureBuffer& buffer,std::uint32_t pixelOffset)
                    {
                        spTextureReadInspectionForAnalysis::Representation representation;
                        representation.kind=spTextureReadInspectionForAnalysis::RepresentationKind::Cross;
                        representation.fieldIndex=observed;representation.width=buffer.GetWidthForAnalysis();
                        representation.height=buffer.GetHeightForAnalysis();representation.format=buffer.GetPixelFormatForAnalysis();
                        representation.auxiliary=buffer.GetPixelSizeForAnalysis();representation.bitsPerPixel=representation.auxiliary*8;
                        representation.mips.push_back({representation.width,representation.height,representation.width,
                            representation.width*representation.auxiliary,representation.height,pixelOffset,
                            static_cast<std::uint32_t>(buffer.GetBufferForAnalysis().size())});
                        inspection->AddRepresentationForAnalysis(std::move(representation));
                    };
                if(!ReadCrossSectionForAnalysis(context,stream,field->payloadSize,[&](const spTextureBuffer& buffer)
                {
                    return InitializeCrossDXForAnalysis(context,buffer,*texture);
                },initialized,error,observeCross))return false;
                finish(spTextureReadInspectionForAnalysis::Outcome::Cross);
                continue;
            }
            if(field->fieldID!=1||!(platform&PCNativeLoadFlagMask))
            {
                if(!local.Skip())return local.Fail("Cannot skip inactive DX texture field");
                finish(spTextureReadInspectionForAnalysis::Outcome::Skipped);continue;
            }
            NativeReadForAnalysis input;
            if(!ReadNativeSectionForAnalysis(context,stream,field->payloadSize,input,error))return false;
            // Capture only the stored records before generation mutates this
            // vector. The input pixel buffers remain owned by the normal reader.
            if(inspection&&observed!=spTextureReadInspectionForAnalysis::NoField)
            {
                spTextureReadInspectionForAnalysis::Representation representation;
                representation.kind=spTextureReadInspectionForAnalysis::RepresentationKind::Native;
                representation.fieldIndex=observed;representation.width=input.width;representation.height=input.height;
                representation.format=input.flags;representation.bitsPerPixel=input.flags?(input.flags==1?4u:8u):32u;
                representation.nativeFlag=input.nativeFlag;representation.field1C=input.field1C;
                for(std::size_t i=0;i<input.mips.size();++i)
                {
                    const auto& mip=input.mips[i];
                    representation.mips.push_back({mip.width,mip.height,mip.width,mip.rowBytes,mip.rows,
                        input.pixelOffsets[i],static_cast<std::uint32_t>(mip.packedBytes.size())});
                }
                inspection->AddRepresentationForAnalysis(std::move(representation));
            }
            const auto width=input.width,height=input.height,flags=input.flags;const auto field1C=input.field1C;
            auto& mips=input.mips;
            while(mips.size()<spDXTexture::FullMipCountForAnalysis(width,height))
            {
                spDXTexture::MipForAnalysis generated;
                const auto& previous=mips.back();
                const bool powerOfTwo=!(previous.width&(previous.width-1))&&!(previous.height&(previous.height-1));
                const bool created=flags?sparkplug::evidence::pc::texture_mips::GenerateNextCompressed(previous,flags,generated)
                    :powerOfTwo?sparkplug::evidence::pc::texture_mips::GenerateNext(previous,generated)
                    :sparkplug::evidence::pc::texture_mips::ResampleRaw(previous,0,
                        std::max(1u,previous.width/2),std::max(1u,previous.height/2),false,generated);
                if(!created)return local.Fail("Cannot generate bounded native mip");
                if(context.pcTexturePitchForAnalysis)generated.physicalPitch=context.pcTexturePitchForAnalysis(context.pcTexturePitchContext,static_cast<std::uint32_t>(mips.size()),generated.rowBytes);
                if(generated.physicalPitch<generated.rowBytes||std::uint64_t(generated.physicalPitch)*generated.rows>16u*1024u*1024u)return local.Fail("Invalid declared generated mip surface pitch");
                mips.push_back(std::move(generated));
            }
            if(!texture->InitializeNativeMipShadowForAnalysis(width,height,flags,field1C,std::move(mips)))
                return local.Fail("Invalid complete native mip chain");
            initialized=true;
            finish(spTextureReadInspectionForAnalysis::Outcome::Native);
        }
        return false;
    }

    const spRTTIRecord& spDXTextureDataSerializer::StaticRTTI() noexcept
    {
        (void)DXTextureDataSerializerRegistered;
        return DXTextureDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spDXTextureDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXTextureDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXTextureDataSerializer::vfunc_18() const noexcept
    {
        return DXTextureDataSerializerRecord;
    }

    spClassID spDXTextureDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spTextureDataSerializer::Field>
    spDXTextureDataSerializer::BuildKnownWritePlanForAnalysis(
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

        std::vector<Field> plan{Field::SourceNone, Field::PlatformType};
        if (nativeSerializationMode == 0 || nativeSerializationMode == 2)
        {
            plan.push_back(Field::CrossPlatform);
        }
        plan.push_back(Field::PlatformSpecific);
        return plan;
    }

    std::uint32_t spDXTextureDataSerializer::PlatformTypeForAnalysis(
        const std::uint32_t nativeSerializationMode) noexcept
    {
        return nativeSerializationMode == 0 || nativeSerializationMode == 2
            ? DXPlatformAndCrossPlatformType
            : DXPlatformType;
    }

    bool spDXTextureDataSerializer::PCLoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PCNativeLoadFlagMask) != 0;
    }

    bool spDXTextureDataSerializer::PS2LoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PS2NativeLoadFlagMask) != 0;
    }

    spDXTextureDataSerializer::NativePayloadHeader
    spDXTextureDataSerializer::DescribeNativePayloadForAnalysis(
        const bool hasPlatformSpecificData,
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint32_t pixelFormat,
        const bool hasPixelData,
        const std::uint32_t mipCount) noexcept
    {
        return {
            mipCount != 0,
            hasPlatformSpecificData,
            width,
            height,
            pixelFormat,
            hasPixelData,
            mipCount,
        };
    }

    spDXTextureDataSerializer::NativeMipHeader
    spDXTextureDataSerializer::BuildNativeMipHeaderForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint32_t rowStride) noexcept
    {
        const auto payloadSize = static_cast<std::uint64_t>(rowStride) * height;
        if (payloadSize > std::numeric_limits<std::uint32_t>::max())
        {
            return {};
        }

        return {
            true,
            width,
            height,
            rowStride,
            static_cast<std::uint32_t>(payloadSize),
        };
    }
}
