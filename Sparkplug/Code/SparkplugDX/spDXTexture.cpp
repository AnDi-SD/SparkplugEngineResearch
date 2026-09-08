#include "spDXTexture.h"
#include <algorithm>
#include <utility>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXTexture>();}
        const spRTTIRecord Record{spDXTexture::ClassID,spTexture::ClassID,"spDXTexture",&spTexture::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXTexture::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXTexture::vfunc_18() const noexcept{return Record;}
    void spDXTexture::AdoptPaletteForAnalysis(std::unique_ptr<spPalette> palette) noexcept
    {palette_=std::move(palette);}
    std::unique_ptr<spBaseObject> spDXTexture::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXTexture>();manager.RegisterClone(*this,*clone);
        return spNamedObject::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    std::uint32_t spDXTexture::RuntimeFormatToCOMForAnalysis(std::uint32_t format) noexcept
    {constexpr std::uint32_t values[]{0x31545844,0x33545844,0x35545844,0x15,0x16,0x29,0x17,0x1a};return format<8?values[format]:format;}
    bool spDXTexture::DescribeMipForAnalysis(std::uint32_t width,std::uint32_t height,std::uint32_t format,MipForAnalysis& mip) noexcept
    {
        if(!width||!height||width>65535||height>65535||format>=8)return false;
        mip.width=width;mip.height=height;
        constexpr std::uint32_t pixelSizes[]{4,4,1,2,2};
        mip.rowBytes=format<3?std::max(1u,width>>2)*(format==0?8u:16u):width*pixelSizes[format-3];
        mip.rows=format<3?std::max(1u,height>>2):height;mip.physicalPitch=mip.rowBytes;
        return std::uint64_t(mip.rowBytes)*mip.rows<=16u*1024u*1024u;
    }
    std::uint32_t spDXTexture::FullMipCountForAnalysis(std::uint32_t width,std::uint32_t height) noexcept
    {
        if(!width||!height||width>65535||height>65535)return 0;
        std::uint32_t count=1;
        while(width!=1||height!=1){width=std::max(1u,width>>1);height=std::max(1u,height>>1);++count;}
        return count;
    }
    bool spDXTexture::InitializeRuntimeMipShadowForAnalysis(std::uint32_t width,std::uint32_t height,
        std::uint32_t format,std::vector<MipForAnalysis> mips)
    {
        if(format>=8||mips.empty()||mips.size()!=FullMipCountForAnalysis(width,height))return false;
        std::uint64_t nativeBytes=0;auto w=width,h=height;
        for(const auto& mip:mips)
        {
            MipForAnalysis expected;
            if(!DescribeMipForAnalysis(w,h,format,expected)||mip.width!=w||mip.height!=h
                ||mip.rowBytes!=expected.rowBytes||mip.rows!=expected.rows||mip.physicalPitch<mip.rowBytes
                ||mip.packedBytes.size()!=std::uint64_t(mip.rowBytes)*mip.rows)return false;
            nativeBytes+=std::uint64_t(format<3?mip.rowBytes:mip.physicalPitch)*mip.rows;
            if(nativeBytes>16u*1024u*1024u)return false;
            w=std::max(1u,w>>1);h=std::max(1u,h>>1);
        }
        // Actual4ABAC0 attach: width/height/format/size/init/field31 only.
        // Base18 and1C are NOT set by this overload, unlike4ABBA0 nativeData.
        ApplyRuntimeAttachmentStateForAnalysis(width,height);
        runtimeFormat_=surfaceFormat_=format;formatInitialized_=true;byteCount_=static_cast<std::uint32_t>(nativeBytes);mips_=std::move(mips);return true;
    }
    bool spDXTexture::InitializeNativeMipShadowForAnalysis(std::uint32_t width,std::uint32_t height,
        std::uint32_t flags,std::uint8_t field1C,std::vector<MipForAnalysis> mips)
    {
        if(flags>3)return false;
        const auto format=flags?flags-1:3u;
        // Validate packed storage using the same rules, without installing the
        // runtime-attach state44/48 that original nativeData4ABBA0 leaves alone.
        spDXTexture validated;
        if(!validated.InitializeRuntimeMipShadowForAnalysis(width,height,format,std::move(mips)))return false;
        ApplyNativeMipStateForAnalysis(width,height,static_cast<std::uint32_t>(validated.mips_.size()),flags,field1C);
        surfaceFormat_=format;mips_=std::move(validated.mips_);return true;
    }
    bool spDXTexture::InitializeCrossMipShadowForAnalysis(std::uint32_t sourceWidth,std::uint32_t sourceHeight,
        std::uint32_t pixelFormat,std::vector<MipForAnalysis> mips)
    {
        if(!sourceWidth||!sourceHeight||sourceWidth>65535||sourceHeight>65535||pixelFormat>=5)return false;
        spDXTexture validated;
        if(!validated.InitializeRuntimeMipShadowForAnalysis(NormalizeDimensionForAnalysis(sourceWidth),
            NormalizeDimensionForAnalysis(sourceHeight),pixelFormat+3,std::move(mips)))return false;
        (void)ApplyBufferStateForAnalysis(sourceWidth,sourceHeight,1,0,true);
        runtimeFormat_=surfaceFormat_=validated.runtimeFormat_;formatInitialized_=true;
        byteCount_=validated.byteCount_;mips_=std::move(validated.mips_);return true;
    }
}
