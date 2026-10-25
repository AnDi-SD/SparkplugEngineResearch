#include "spDXTexture.h"
#include "spDXCubeTexture.h"
#include "spDXRenderer.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        DXResourceResultForAnalysis ReplacePalette(
            std::unique_ptr<spPalette>& current, std::unique_ptr<spPalette>& incoming,
            DXPalettePoolForAnalysis& pool)
        {
            if (current)
            {
                // Native dereferences NULL new palette or stores a deleted
                // same-object pointer. Those unsafe domains are refused.
                if (!incoming || current.get() == incoming.get()) return {};
                const auto recycled = spDXRenderer::RecyclePaletteIndexForAnalysis(pool, *incoming);
                if (!recycled.completed) return {};
                current.reset();
            }
            current = std::move(incoming);
            return {true, false}; // Original setter is void, not native AL1.
        }

        bool ValidPaletteCohort(const DXTextureResourceStateForAnalysis& state,
            const std::unique_ptr<spPalette>& current, const std::unique_ptr<spPalette>& incoming)
        {
            return state.palette == reinterpret_cast<DXResourceHandleForAnalysis>(current.get())
                && (!current || (incoming && current.get() != incoming.get()));
        }

        DXResourceResultForAnalysis InstallAttachmentPalette(
            DXTextureResourceStateForAnalysis& state,
            std::unique_ptr<spPalette>& current, std::unique_ptr<spPalette>& incoming,
            DXPalettePoolForAnalysis& pool, const DXResourceHandleForAnalysis rendererDevice,
            const DXTextureResourceAPIForAnalysis& api)
        {
            const auto replacement = ReplacePalette(current, incoming, pool);
            if (!replacement.completed) return replacement;
            state.palette = reinterpret_cast<DXResourceHandleForAnalysis>(current.get());
            if (current && current->GetIndexForAnalysis() == 0xFFFFFFFFu)
            {
                const auto registered = spDXRenderer::RegisterPaletteForAnalysis(
                    pool, rendererDevice, *current, api.setPaletteEntries, api.paletteContext);
                if (!registered.completed) return {};
            }
            return {true, false};
        }
    }

    DXResourceResultForAnalysis spDXTexture::ReplaceDevicePaletteForAnalysis(
        DXPalettePoolForAnalysis& pool, std::unique_ptr<spPalette>& incoming)
    {
        return ReplacePalette(palette_, incoming, pool);
    }

    DXResourceResultForAnalysis spDXCubeTexture::ReplaceDevicePaletteForAnalysis(
        DXPalettePoolForAnalysis& pool, std::unique_ptr<spPalette>& incoming)
    {
        return ReplacePalette(palette_, incoming, pool);
    }

    DXResourceResultForAnalysis spDXTexture::AttachOwnedDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const std::uint32_t width, const std::uint32_t height, const std::uint32_t format,
        const DXResourceHandleForAnalysis texture, std::unique_ptr<spPalette>& incoming,
        const std::uint32_t byteCount, DXPalettePoolForAnalysis& pool,
        const DXResourceHandleForAnalysis rendererDevice, const DXTextureResourceAPIForAnalysis& api)
    {
        if (!ValidPaletteCohort(state, palette_, incoming) || texture == 0 || !api.addRef
            || (state.texture != 0 && !api.release)) return {};
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = texture;
        const auto installed = InstallAttachmentPalette(state, palette_, incoming, pool, rendererDevice, api);
        if (!installed.completed) return installed;
        state.width = width;
        state.height = height;
        state.texture = texture;
        state.format = format;
        state.field31 = 0;
        state.initialized = 1;
        if (byteCount != 0) state.byteCount = byteCount;
        else
        {
            const auto measured = MeasureDeviceByteCountForAnalysis(state, api);
            if (!measured.completed) return measured;
        }
        (void)api.addRef(state.texture);
        return {true, false};
    }

    DXResourceResultForAnalysis spDXCubeTexture::AttachOwnedDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const std::uint32_t width, const std::uint32_t height, const std::uint32_t format,
        const DXResourceHandleForAnalysis texture, std::unique_ptr<spPalette>& incoming,
        const std::uint32_t byteCount, DXPalettePoolForAnalysis& pool,
        const DXResourceHandleForAnalysis rendererDevice, DXTextureResourceGlobalsForAnalysis& globals,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (!ValidPaletteCohort(state, palette_, incoming) || texture == 0 || !api.addRef
            || !api.getLevelCount || (state.texture != 0 && !api.release)) return {};
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = texture;
        const auto installed = InstallAttachmentPalette(state, palette_, incoming, pool, rendererDevice, api);
        if (!installed.completed) return installed;
        state.width = width;
        state.height = height;
        state.texture = texture;
        state.format = format;
        state.field31 = 0;
        state.byteCount = byteCount;
        state.initialized = 1;
        (void)api.addRef(state.texture);
        const auto regenerated = RegenerateDeviceMipsForAnalysis(state, globals, api);
        return {regenerated.completed, false, 0, regenerated.acquiredMipSurfaces};
    }

    bool spDXTexture::ReleaseDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const DXTextureResourceAPIForAnalysis& api)
    {
        //004AB170 receives the secondary interface (complete object+14).
        if (state.texture != 0 && !api.release) return false;
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = 0; // Cleared even when COM Release returned nonzero.
        return true;       // Host completion; original has no defined bool.
    }

    bool spDXCubeTexture::ReleaseDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const DXTextureResourceAPIForAnalysis& api)
    {
        //004B9390 unconditionally releases stored device38 and does not clear
        // that field. Repeating reset therefore repeats this Release.
        if (!api.release || state.device == 0) return false;
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = 0;
        (void)api.release(state.device);
        return true;
    }

    std::uint32_t spDXTexture::COMFormatToRuntimeForAnalysis(
        const std::uint32_t format) noexcept
    {
        //004AAE30, including unknown-format pass-through.
        switch (format)
        {
        case 0x31545844: return 0;
        case 0x33545844: return 1;
        case 0x35545844: return 2;
        case 0x15: return 3;
        case 0x16: return 4;
        case 0x29: return 5;
        case 0x17: return 6;
        case 0x1A: return 7;
        default: return format;
        }
    }

    DXResourceResultForAnalysis spDXTexture::AttachDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const std::uint32_t width, const std::uint32_t height,
        const std::uint32_t format, const DXResourceHandleForAnalysis texture,
        const DXResourceHandleForAnalysis palette,
        const std::uint32_t byteCount, const DXTextureResourceAPIForAnalysis& api)
    {
        // Field40 is spPalette; nonzero palette ownership/index-pool operations
        // are a separate domain. No successful internal callback is supplied.
        if (state.palette != 0 || palette != 0
            || texture == 0 || !api.addRef || (state.texture != 0 && !api.release))
            return {};
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = texture; // Before native vfunc3C palette setter.
        state.palette = palette;
        state.width = width;
        state.height = height;
        state.texture = texture;
        state.format = format;
        state.field31 = 0;
        state.initialized = 1;
        if (byteCount != 0) state.byteCount = byteCount;
        else
        {
            const auto measured = MeasureDeviceByteCountForAnalysis(state, api);
            if (!measured.completed) return measured;
        }
        (void)api.addRef(state.texture);
        //004ABAC0 is void; nativeReturn is not asserted for it.
        return {true, false, 0};
    }

    DXResourceResultForAnalysis spDXTexture::MeasureDeviceByteCountForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.texture == 0 || !api.getLevelCount) return {};
        // Protected bridge13B148C resolves404BC6: mov [esi+48],0; jmp4AAC62.
        state.byteCount = 0;
        const auto levels = api.getLevelCount(state.texture);
        if (levels > api.maximumMipLevels) return {};
        if (levels != 0 && (!api.getSurfaceLevel || !api.getSurfaceDescription || !api.release))
            return {};
        for (std::uint32_t level = 0; level != levels; ++level)
        {
            DXResourceHandleForAnalysis surface = 0;
            (void)api.getSurfaceLevel(state.texture, level, surface);
            if (surface == 0) return {};
            std::optional<DXSurfaceDescriptionForAnalysis> description;
            (void)api.getSurfaceDescription(surface, description);
            if (!description) return {false, false, 0, {surface, 0}};
            if (description->format == 0x31545844
                || description->format == 0x33545844 || description->format == 0x35545844)
            {
                auto blocksX = description->width >> 2;
                auto blocksY = description->height >> 2;
                if (blocksX < 1) blocksX = 1;
                if (blocksY < 1) blocksY = 1;
                const std::uint32_t blockBytes = description->format == 0x31545844 ? 8u : 16u;
                state.byteCount += blocksX * blocksY * blockBytes;
            }
            else
            {
                if (!api.lockSurface || !api.unlockSurface)
                    return {false, false, 0, {surface, 0}};
                std::optional<DXLockedRectForAnalysis> locked;
                (void)api.lockSurface(surface, locked, 0, 0);
                if (!locked) return {false, false, 0, {surface, 0}};
                state.byteCount += locked->pitch * description->height;
                (void)api.unlockSurface(surface);
            }
            (void)api.release(surface);
        }
        return {true, true, 0};
    }

    DXResourceResultForAnalysis spDXCubeTexture::RegenerateDeviceMipsForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        DXTextureResourceGlobalsForAnalysis& globals,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.texture == 0 || !api.getLevelCount) return {};
        const auto levelCount = api.getLevelCount(state.texture);
        // Native unsigned count-1 would wrap when zero. Bound the captured
        // external API instead of claiming that unsafe loop is completed.
        if (levelCount == 0 || levelCount > api.maximumMipLevels) return {};
        if (levelCount > 1 && (!api.getCubeMapSurface
            || !api.getSurfaceDescription || !api.loadSurfaceFromSurface || !api.release))
            return {};
        globals.mipProcessing = 1;
        for (std::uint32_t face = 0; face != 6; ++face)
        {
            for (std::uint32_t level = 0; level + 1 < levelCount; ++level)
            {
                DXResourceHandleForAnalysis source = 0, destination = 0;
                (void)api.getCubeMapSurface(state.texture, face, level, source);
                if (source == 0) return {}; // Original next dereference unsafe.
                std::optional<DXSurfaceDescriptionForAnalysis> sourceDescription;
                (void)api.getSurfaceDescription(source, sourceDescription);
                if (!sourceDescription) return {false, false, 0, {source, 0}};
                const DXResourceRectForAnalysis sourceRect{
                    0, 0, sourceDescription->width, sourceDescription->height};
                (void)api.getCubeMapSurface(state.texture, face, level + 1, destination);
                if (destination == 0) return {false, false, 0, {source, 0}};
                std::optional<DXSurfaceDescriptionForAnalysis> destinationDescription;
                (void)api.getSurfaceDescription(destination, destinationDescription);
                if (!destinationDescription) return {false, false, 0, {source, destination}};
                const DXResourceRectForAnalysis destinationRect{
                    0, 0, destinationDescription->width, destinationDescription->height};
                (void)api.loadSurfaceFromSurface(destination, destinationRect,
                    source, sourceRect, 4, 0);
                (void)api.release(source);
                (void)api.release(destination);
            }
        }
        globals.mipProcessing = 0;
        return {true, true, 0};
    }

    DXResourceResultForAnalysis spDXCubeTexture::AttachDeviceTextureForAnalysis(
        DXTextureResourceStateForAnalysis& state,
        const std::uint32_t width, const std::uint32_t height,
        const std::uint32_t format, const DXResourceHandleForAnalysis texture,
        const DXResourceHandleForAnalysis palette,
        const std::uint32_t byteCount, DXTextureResourceGlobalsForAnalysis& globals,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.palette != 0 || palette != 0 || texture == 0
            || !api.addRef || !api.getLevelCount || (state.texture != 0 && !api.release))
            return {};
        if (state.texture != 0) (void)api.release(state.texture);
        state.texture = texture;
        state.palette = palette;
        state.width = width;
        state.height = height;
        state.texture = texture;
        state.format = format; // Cube stores raw D3DFORMAT, unlike ordinary.
        state.field31 = 0;
        state.byteCount = byteCount;
        state.initialized = 1;
        (void)api.addRef(texture);
        const auto regenerated = RegenerateDeviceMipsForAnalysis(state, globals, api);
        //004B9630 ignores the helper's AL, but an incomplete host helper cannot
        // be silently treated as a native completed call.
        return {regenerated.completed, false, 0, regenerated.acquiredMipSurfaces};
    }
}
