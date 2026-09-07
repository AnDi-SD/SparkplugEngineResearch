#include "spTextureData.h"

#include <limits>
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextureData()
        {
            return std::make_unique<spTextureData>();
        }

        const spRTTIRecord TextureDataRecord{
            spTextureData::ClassID,
            spTexture::ClassID,
            "spTextureData",
            &spTexture::StaticRTTI(),
            &CreateTextureData,
            nullptr,
        };

        const bool TextureDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TextureDataRecord);
    }

    const spRTTIRecord& spTextureData::StaticRTTI() noexcept
    {
        (void)TextureDataRegistered;
        return TextureDataRecord;
    }

    bool spTextureData::SetNativeMipDataForAnalysis(std::uint32_t width,std::uint32_t height,
        std::uint32_t flags,std::uint8_t field1C,std::vector<NativeMipForAnalysis> mips)
    {
        if(!width||!height||width>65535||height>65535||(width&(width-1))||(height&(height-1))||flags>3||mips.empty())return false;
        auto w=width,h=height;std::uint64_t total=0;bool ended=false;
        for(const auto& mip:mips)
        {
            const auto stride=flags?std::max(1u,w>>2)*(flags==1?8u:16u):w*4u;
            const auto rows=flags?std::max(1u,h>>2):h;const auto size=std::uint64_t(stride)*rows;
            total+=size;
            if(ended||mip.width!=w||mip.rows!=rows||mip.rowStride!=stride||mip.bytes.size()!=size||total>16u*1024u*1024u)return false;
            ended=w==1&&h==1;w=std::max(1u,w>>1);h=std::max(1u,h>>1);
        }
        // Host initialization of proven fields; no claim of native vector creation.
        ApplyNativeMipStateForAnalysis(width,height,static_cast<std::uint32_t>(mips.size()),flags,field1C);
        field68_=true;nativeMips_=std::move(mips);return true;
    }

    std::unique_ptr<spBaseObject> spTextureData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTextureData>();
        manager.RegisterClone(*this, *clone);

        // Native spTextureData does not override the inherited name-copy
        // routine. Its clone therefore retains the name but leaves texture
        // state, embedded pixels, flags and platform containers blank.
        return spNamedObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTextureData::vfunc_18() const noexcept
    {
        return TextureDataRecord;
    }

    bool spTextureData::InitializeFromTextureBufferForAnalysis(
        const spTextureBuffer& source,
        const std::uint8_t field1C,
        const std::uint32_t textureFlags,
        const bool normalizeDimensions)
    {
        const auto sourceWidth = source.GetWidthForAnalysis();
        const auto sourceHeight = source.GetHeightForAnalysis();
        (void)ApplyBufferStateForAnalysis(
            sourceWidth,
            sourceHeight,
            field1C,
            textureFlags,
            normalizeDimensions);

        if (!source.IsInitializedForAnalysis()
            || sourceWidth > std::numeric_limits<std::uint16_t>::max()
            || sourceHeight > std::numeric_limits<std::uint16_t>::max()
            || source.GetDepthForAnalysis()
                > std::numeric_limits<std::uint16_t>::max()
            || source.HasAuxiliaryObjectForAnalysis()
            || !textureBuffer_.InitializeForAnalysis(
                static_cast<std::uint16_t>(sourceWidth),
                static_cast<std::uint16_t>(sourceHeight),
                static_cast<std::uint16_t>(source.GetDepthForAnalysis()),
                source.GetPixelFormatForAnalysis()))
        {
            return false;
        }

        return textureBuffer_.SetDataForAnalysis(
            source.GetBufferForAnalysis());
    }

    spTextureBuffer& spTextureData::GetTextureBufferForAnalysis() noexcept
    {
        return textureBuffer_;
    }

    const spTextureBuffer&
        spTextureData::GetTextureBufferForAnalysis() const noexcept
    {
        return textureBuffer_;
    }

    bool spTextureData::GetField68ForAnalysis() const noexcept
    {
        return field68_;
    }

    void spTextureData::SetField68ForAnalysis(const bool value) noexcept
    {
        field68_ = value;
    }

    bool spTextureData::GetFieldAfterFirstContainerForAnalysis() const noexcept
    {
        return fieldAfterFirstContainer_;
    }

    void spTextureData::SetFieldAfterFirstContainerForAnalysis(
        const bool value) noexcept
    {
        fieldAfterFirstContainer_ = value;
    }
}
