#pragma once

// Captured PC COM resource state. These carriers are not native object layouts
// or a graphics backend. Only COM and D3DX endpoints are supplied by the host;
// attachment, metadata, release and mip traversal remain recovered source.
#include "../../Analysis/Host/spRenderTargetSurfaceHost.h"
#include "spDXPalettePool.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace sparkplug::reconstruction
{
    using DXResourceHandleForAnalysis = std::uintptr_t;
    using DXSurfaceDescriptionForAnalysis =
        sparkplug::analysis::host::RenderTargetSurfaceDescription;

    struct DXResourceRectForAnalysis final
    {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };

    struct DXLockedRectForAnalysis final
    {
        // Keep the pitch's raw32 bits: native IMUL/addition wrap at32 bits.
        std::uint32_t pitch = 0;
        DXResourceHandleForAnalysis bits = 0;
        // Only the native-data CPU-copy entry consumes pBits. Its provider
        // supplies an actual writable host span, distinct from source storage.
        // Measurement entries consume pitch only and do not require this bound.
        std::size_t accessibleBytes = 0;
    };

    struct DXTextureResourceAPIForAnalysis final
    {
        using Handle = DXResourceHandleForAnalysis;
        // HRESULT is deliberately separate from the output argument. Original
        // target/attachment code ignores it, including when failure wrote data.
        std::function<std::int32_t(Handle, std::uint32_t, std::uint32_t,
            std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
            Handle&, Handle)> createTexture;
        std::function<std::int32_t(Handle, std::uint32_t, std::uint32_t,
            std::uint32_t, std::uint32_t, std::uint32_t, Handle&, Handle)>
            createCubeTexture;
        std::function<std::int32_t(Handle, std::uint32_t, Handle&)>
            getSurfaceLevel;
        std::function<std::int32_t(Handle, std::uint32_t, std::uint32_t, Handle&)>
            getCubeMapSurface;
        // An empty optional records that no initialized descriptor was written;
        // consuming uninitialized original stack storage is not emulated.
        std::function<std::int32_t(Handle,
            std::optional<DXSurfaceDescriptionForAnalysis>&)> getSurfaceDescription;
        std::function<std::uint32_t(Handle)> addRef;
        std::function<std::uint32_t(Handle)> release;
        std::function<std::uint32_t(Handle)> getLevelCount;
        std::function<std::int32_t(Handle, std::optional<DXLockedRectForAnalysis>&,
            Handle, std::uint32_t)> lockSurface;
        std::function<std::int32_t(Handle)> unlockSurface;
        // Actual device endpoint used by the typed palette-owning attachment;
        // its renderer device is supplied separately from saved backing38.
        DXPaletteEntriesForAnalysis setPaletteEntries = nullptr;
        void* paletteContext = nullptr;
        // D3DXLoadSurfaceFromSurface: palettes NULL, rectangles explicit,
        // filter4, color-key0. SDK filtering arithmetic remains external.
        std::function<std::int32_t(Handle, const DXResourceRectForAnalysis&,
            Handle, const DXResourceRectForAnalysis&, std::uint32_t,
            std::uint32_t)> loadSurfaceFromSurface;
        std::uint32_t maximumMipLevels = 32; // HOST bound, not a native limit.
    };

    struct DXTextureResourceStateForAnalysis final
    {
        DXResourceHandleForAnalysis device = 0;       // native38
        DXResourceHandleForAnalysis texture = 0;      // native3C
        DXResourceHandleForAnalysis palette = 0; // native40, spPalette pointer
        std::uint32_t width = 0, height = 0;           // native28/2C
        std::uint32_t format = 0, byteCount = 0;       // native44/48
        std::uint8_t initialized = 0, field31 = 0;    // native24/31
        // Native-data upload writes these base fields, while attachment does
        // not. They remain captured values, separate from a CPU mip shadow.
        std::uint32_t field18 = 0, textureFlags = 0;  // native18/20
        std::uint8_t field1C = 0;
    };

    struct DXTextureResourceGlobalsForAnalysis final
    {
        std::uint8_t mipProcessing = 0; // shared original byte73FE6B
    };

    struct DXRenderTargetStateForAnalysis final
    {
        std::uint32_t width = 256, height = 256, pixelFormat = 0;
        DXTextureResourceStateForAnalysis backing;
        DXResourceHandleForAnalysis surface = 0;
    };

    struct DXCubeRenderTargetStateForAnalysis final
    {
        std::uint32_t width = 256, height = 256, pixelFormat = 0;
        // Native cube release reads the +28 relationship, while initialization
        // writes through +18. This bounded carrier requires them to identify
        // the same backing object; it does not invent general alias behavior.
        DXTextureResourceStateForAnalysis backing;
        std::array<DXResourceHandleForAnalysis, 6> surfaces{};
    };

    struct DXResourceResultForAnalysis final
    {
        bool completed = false;
        bool nativeReturn = false;
        // Observed local output, not an additional owning native field. Retain
        // it on incomplete host paths so a created COM reference is not hidden.
        DXResourceHandleForAnalysis createdTexture = 0;
        // Unreleased mip-local outputs at an incomplete host boundary. They
        // are observations for caller cleanup, not extra native owners.
        std::array<DXResourceHandleForAnalysis, 2> acquiredMipSurfaces{};
    };
}
