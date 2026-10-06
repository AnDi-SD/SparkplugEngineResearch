#pragma once

// Original PC class identity; source path inferred. Portable storage is an
// explicit HOST CPU shadow of PC004B9630, not a COM device or GPU resource.
#include "../Sparkplug/spCubeTexture.h"
#include "spDXTexture.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spDXCubeTexture final : public spCubeTexture
    {
    public:
        static constexpr spClassID ClassID = 0x5C542AD9;
        static constexpr std::size_t FaceCount = 6;
        // Cube payloads store raw D3DFORMAT values. Unlike ordinary DX texture
        // payloads, native004B86C0 forwards this word without enum conversion.
        static constexpr std::array<std::uint32_t, 8> NativeRuntimeFormatsForAnalysis{
            0x31545844, 0x33545844, 0x35545844, 0x15, 0x16, 0x29, 0x17, 0x1a};
        [[nodiscard]] static bool IsSupportedRuntimeFormatForAnalysis(std::uint32_t format) noexcept;
        using MipForAnalysis = spDXTexture::MipForAnalysis;
        using FacesForAnalysis = std::array<std::vector<MipForAnalysis>, FaceCount>;
        // HOST dependency for native004B91D0's D3DXLoadSurfaceFromSurface.
        // Required for more than one level. The callback must regenerate all
        // lower levels and retain each descriptor and packed storage extent.
        // Its context must outlive this object. No filtering default is guessed.
        using MipRegeneratorForAnalysis = bool (*)(void*, std::uint32_t, FacesForAnalysis&);
        void SetMipRegeneratorForAnalysis(void* context, MipRegeneratorForAnalysis callback) noexcept
        { mipContext_ = context; mipRegenerator_ = callback; }
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        // PC004B8560 supplies row bytes. PC004B8833/004B873C classify DXT1,
        // DXT3 and DXT4 for row count, accidentally omitting DXT5. Preserve
        // that native file behavior; no corrected block count is substituted.
        [[nodiscard]] static bool DescribeSerializedMipForAnalysis(
            std::uint32_t dimension, std::uint32_t runtimeFormat,
            MipForAnalysis& mip) noexcept;
        [[nodiscard]] bool InitializeRuntimeFacesForAnalysis(
            std::uint32_t dimension, std::uint32_t runtimeFormat, FacesForAnalysis faces);
        [[nodiscard]] const FacesForAnalysis& GetFacesForAnalysis() const noexcept { return faces_; }
        [[nodiscard]] bool HasInitializedRuntimeFormatForAnalysis() const noexcept { return formatInitialized_; }
        [[nodiscard]] std::uint32_t GetRuntimeFormatForAnalysis() const noexcept { return runtimeFormat_; }
        [[nodiscard]] std::uint32_t GetNativeByteCountForAnalysis() const noexcept { return byteCount_; }
        [[nodiscard]] const spPalette* GetPaletteForAnalysis() const noexcept { return palette_.get(); }
        void AdoptPaletteForAnalysis(std::unique_ptr<spPalette> palette) noexcept;
        [[nodiscard]] static constexpr bool HasLiveGraphicsBackendForAnalysis() noexcept { return false; }

    private:
        // Native ctor initializes3C/40/48 but leaves44 uninitialized. The
        // boolean models that condition; zero host storage is no native default.
        std::uint32_t runtimeFormat_ = 0, byteCount_ = 0;
        bool formatInitialized_ = false;
        FacesForAnalysis faces_;
        void* mipContext_ = nullptr;
        MipRegeneratorForAnalysis mipRegenerator_ = nullptr;
        // HOST RAII replaces native unsafe palette deletion/renderer registry.
        std::unique_ptr<spPalette> palette_;
    };
}
