#pragma once
// Original PC class identity. Header/TU path inferred, not recovered.
#include "../Sparkplug/spTexture.h"
#include "../Sparkplug/spPalette.h"
#include <cstddef>
#include <vector>

namespace sparkplug::reconstruction
{
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
        [[nodiscard]] static constexpr bool HasLiveGraphicsBackendForAnalysis() noexcept{return false;}
    private:
        // Native ctor does not write44. Zero host storage is NOT native default.
        std::uint32_t runtimeFormat_=0,byteCount_=0;
        std::uint32_t surfaceFormat_=0; // analytical COM-surface descriptor, NOT native field44
        bool formatInitialized_=false;
        std::vector<MipForAnalysis> mips_;
        std::unique_ptr<spPalette> palette_;
    };
}
