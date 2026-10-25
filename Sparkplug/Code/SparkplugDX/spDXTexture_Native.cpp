#include "spDXTexture.h"
#include "../Sparkplug/spTextureData.h"
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    DXResourceResultForAnalysis spDXTexture::UploadNativeDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,const spTextureData& data,
        DXTextureResourceGlobalsForAnalysis& globals,const DXTextureResourceAPIForAnalysis& api)
    {
        const auto& mips=data.GetNativeMipsForAnalysis();
        const auto flags=data.GetTextureFlagsForAnalysis();
        // Stable decoded TextureData and complete external COM functions are
        // host preconditions. The nativeFlag==0 virtual cross-upload and the
        // native flags>3 source-pointer-as-format case are not invented here.
        if(!data.GetField68ForAnalysis()||flags>3||mips.empty()||mips.size()>api.maximumMipLevels
            ||!api.createTexture||!api.release||!api.getSurfaceLevel||!api.getSurfaceDescription
            ||!api.lockSurface||!api.unlockSurface||!api.getLevelCount||state.device==0)return {};
        for(const auto& mip:mips)
            if(std::uint64_t(mip.rowStride)*mip.rows!=mip.bytes.size())return {};
        if(state.texture!=0)(void)api.release(state.texture);
        state.texture=0;
        constexpr std::uint32_t formats[]{0x15,0x31545844,0x33545844,0x35545844};
        // Direct owned output slot, levels0/usage0/pool1/sharedNULL. Ignore HR.
        (void)api.createTexture(state.device,data.GetWidthForAnalysis(),data.GetHeightForAnalysis(),
            0,0,formats[flags],1,state.texture,0);
        if(state.texture==0)return {};
        for(std::size_t level=0;level<mips.size();++level)
        {
            const auto& mip=mips[level];DXResourceHandleForAnalysis surface=0;
            (void)api.getSurfaceLevel(state.texture,static_cast<std::uint32_t>(level),surface);
            if(surface==0)return {false,false,state.texture};
            std::optional<DXSurfaceDescriptionForAnalysis> unusedDescription;
            (void)api.getSurfaceDescription(surface,unusedDescription);
            // Unlike byte-count measurement,4ABBA0 never reads this descriptor.
            std::optional<DXLockedRectForAnalysis> locked;
            (void)api.lockSurface(surface,locked,0,0x10);
            if(!locked)return {false,false,state.texture,{surface,0}};
            const std::uint64_t required=mip.rows
                ?std::uint64_t(mip.rows-1)*locked->pitch+mip.rowStride:0;
            // The declared host span bounds memory safety. Arbitrary native
            // row overlap, pointer wrap and aliasing are not emulated.
            if(locked->pitch<mip.rowStride||required>locked->accessibleBytes
                ||(required&&locked->bits==0)
                ||required>std::numeric_limits<std::uintptr_t>::max()-locked->bits)
                return {false,false,state.texture,{surface,0}};
            const auto destinationBegin=locked->bits;
            const auto destinationEnd=destinationBegin+static_cast<std::uintptr_t>(required);
            for(const auto& source:mips)
            {
                const auto begin=reinterpret_cast<std::uintptr_t>(source.bytes.data());
                const auto end=begin+source.bytes.size();
                if(destinationBegin<end&&begin<destinationEnd)
                    return {false,false,state.texture,{surface,0}};
            }
            auto* destination=reinterpret_cast<std::byte*>(locked->bits);
            for(std::uint32_t row=0;row<mip.rows;++row)
            {
                if(mip.rowStride)std::memcpy(destination+std::size_t(row)*locked->pitch,
                    mip.bytes.data()+std::size_t(row)*mip.rowStride,mip.rowStride);
            }
            (void)api.unlockSurface(surface);
            (void)api.release(surface);
        }
        // Actual4AB030: GetLevelCount precedes global1. On the represented
        // full-provided branch it then clears the byte and returns true. The
        // other branch enters internal61039A, which is not a successful hook.
        const auto count=api.getLevelCount(state.texture);
        globals.mipProcessing=1;
        if(count==0||count>api.maximumMipLevels||mips.size()<count)
            return {false,false,state.texture};
        globals.mipProcessing=0;
        state.field18=api.getLevelCount(state.texture);
        state.field1C=data.GetField1CForAnalysis();
        state.textureFlags=flags;
        state.width=data.GetWidthForAnalysis();
        state.height=data.GetHeightForAnalysis();
        state.initialized=1;
        // Native format44/byteCount48/field31/palette40 remain untouched.
        return {true,true,state.texture};
    }
}
