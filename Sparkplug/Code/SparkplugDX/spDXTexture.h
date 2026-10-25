#pragma once
// Original PC class identity. Header/TU path inferred, not recovered.
#include "../Sparkplug/spTexture.h"
#include "../Sparkplug/spPalette.h"
#include "spDXTextureResource.h"
#include "spDXPalettePool.h"
#include <cstddef>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTextureData;
    // Host CPU shadow of proven runtime texture state, NOT a live COM resource.
    class spDXTexture final : public spTexture
    {
    public:
        static constexpr spClassID ClassID=0x3F3651B6;
        struct MipForAnalysis final
        {
            std::uint32_t width=0,height=0,rowBytes=0,rows=0,physicalPitch=0;
            std::vector<std::byte> packedBytes;
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] static std::uint32_t RuntimeFormatToCOMForAnalysis(std::uint32_t) noexcept;
        [[nodiscard]] static bool DescribeMipForAnalysis(std::uint32_t width,std::uint32_t height,
            std::uint32_t runtimeFormat,MipForAnalysis&) noexcept;
        [[nodiscard]] static std::uint32_t FullMipCountForAnalysis(std::uint32_t width,std::uint32_t height) noexcept;
        [[nodiscard]] bool InitializeRuntimeMipShadowForAnalysis(std::uint32_t width,std::uint32_t height,
            std::uint32_t runtimeFormat,std::vector<MipForAnalysis>);
        [[nodiscard]] bool InitializeNativeMipShadowForAnalysis(std::uint32_t width,std::uint32_t height,
            std::uint32_t nativeFlags,std::uint8_t field1C,std::vector<MipForAnalysis>);
        [[nodiscard]] bool InitializeCrossMipShadowForAnalysis(std::uint32_t sourceWidth,std::uint32_t sourceHeight,
            std::uint32_t pixelFormat,std::vector<MipForAnalysis>);
        [[nodiscard]] bool HasInitializedRuntimeFormatForAnalysis() const noexcept{return formatInitialized_;}
        [[nodiscard]] std::uint32_t GetRuntimeFormatForAnalysis() const noexcept{return runtimeFormat_;}
        [[nodiscard]] std::uint32_t GetSurfaceFormatForAnalysis() const noexcept{return surfaceFormat_;}
        [[nodiscard]] std::uint32_t GetNativeByteCountForAnalysis() const noexcept{return byteCount_;}
        [[nodiscard]] const std::vector<MipForAnalysis>& GetMipsForAnalysis() const noexcept{return mips_;}
        [[nodiscard]] const spPalette* GetPaletteForAnalysis() const noexcept{return palette_.get();}
        // Explicit HOST ownership policy: original setter aliases/delete and
        // native DX destructor leaks palette. Do not reproduce those hazards.
        void AdoptPaletteForAnalysis(std::unique_ptr<spPalette> palette) noexcept;
        // Original4B93C0 on valid, distinct C++ owners. With an old palette it
        // recycles the NEW palette's index before destroying the old palette.
        // Host refuses null-new/old-present and alias hazards before mutation.
        [[nodiscard]] DXResourceResultForAnalysis ReplaceDevicePaletteForAnalysis(
            DXPalettePoolForAnalysis&,std::unique_ptr<spPalette>& incoming);
        [[nodiscard]] static constexpr bool HasLiveGraphicsBackendForAnalysis() noexcept{return false;}
        // Original004AB170/004ABAC0 on explicit captured COM state. This is
        // separate from the CPU mip shadow and performs no invented API call.
        [[nodiscard]] static bool ReleaseDeviceTextureForAnalysis(
            DXTextureResourceStateForAnalysis&, const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static DXResourceResultForAnalysis AttachDeviceTextureForAnalysis(
            DXTextureResourceStateForAnalysis&, std::uint32_t width, std::uint32_t height,
            std::uint32_t runtimeFormat, DXResourceHandleForAnalysis texture,
            DXResourceHandleForAnalysis palette, std::uint32_t byteCount,
            const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static std::uint32_t COMFormatToRuntimeForAnalysis(std::uint32_t) noexcept;
        // Whole004AAC50: resets48 then measures each acquired mip surface.
        // Compressed formats use floor block counts; other formats lock for
        // physical pitch. No pixel bytes are read by this native operation.
        [[nodiscard]] static DXResourceResultForAnalysis MeasureDeviceByteCountForAnalysis(
            DXTextureResourceStateForAnalysis&, const DXTextureResourceAPIForAnalysis&);
        // Original4ABBA0 nativeFlag branch: real Create/Lock/CPU row copy and
        // metadata. Supplied native mip records are reused from TextureData.
        // Cross conversion and missing-mip producers are incomplete boundaries.
        [[nodiscard]] static DXResourceResultForAnalysis UploadNativeDeviceTextureForAnalysis(
            DXTextureResourceStateForAnalysis&,const spTextureData&,
            DXTextureResourceGlobalsForAnalysis&,const DXTextureResourceAPIForAnalysis&);
        // Whole004ABAC0 with this object's actual palette owner and captured
        // COM/metadata state. State.palette must identify GetPaletteForAnalysis.
        // rendererDevice is the current global renderer device, not backing38.
        [[nodiscard]] DXResourceResultForAnalysis AttachOwnedDeviceTextureForAnalysis(
            DXTextureResourceStateForAnalysis&, std::uint32_t width, std::uint32_t height,
            std::uint32_t runtimeFormat, DXResourceHandleForAnalysis texture,
            std::unique_ptr<spPalette>& incoming, std::uint32_t byteCount,
            DXPalettePoolForAnalysis&, DXResourceHandleForAnalysis rendererDevice,
            const DXTextureResourceAPIForAnalysis&);
    private:
        // Native ctor does not write44. Zero host storage is NOT native default.
        std::uint32_t runtimeFormat_=0,byteCount_=0;
        std::uint32_t surfaceFormat_=0; // analytical COM-surface descriptor, NOT native field44
        bool formatInitialized_=false;
        std::vector<MipForAnalysis> mips_;
        std::unique_ptr<spPalette> palette_;
    };
}
