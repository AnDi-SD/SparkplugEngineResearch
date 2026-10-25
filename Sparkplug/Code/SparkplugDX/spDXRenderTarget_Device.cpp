#include "spDXRenderTarget.h"
#include "spDXCubeRenderTarget.h"
#include "spDXTexture.h"
#include "spDXCubeTexture.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    bool spDXRenderTarget::ReleaseDeviceTargetForAnalysis(
        DXRenderTargetStateForAnalysis& state,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.surface == 0) return true;
        if (!api.release) return false;
        if (state.backing.texture != 0) (void)api.release(state.backing.texture);
        if (!spDXTexture::ReleaseDeviceTextureForAnalysis(state.backing, api)) return false;
        (void)api.release(state.surface);
        state.surface = 0;
        return true;
    }

    DXResourceResultForAnalysis spDXRenderTarget::InitializeDeviceTargetForAnalysis(
        DXRenderTargetStateForAnalysis& state,
        const DXResourceHandleForAnalysis device,
        const std::uint32_t width, const std::uint32_t height,
        const std::uint32_t pixelFormat, const DXTextureResourceAPIForAnalysis& api)
    {
        if (!ReleaseDeviceTargetForAnalysis(state, api)) return {};
        const auto format = static_cast<eTBPixelFormat>(pixelFormat);
        if (!IsSupportedFormatForAnalysis(format)) return {true, false, 0};
        if (device == 0 || !api.createTexture || !api.getSurfaceLevel
            || !api.getSurfaceDescription || state.backing.palette != 0)
            return {};
        DXResourceHandleForAnalysis texture = 0;
        const auto d3dFormat = ToD3DFormatForAnalysis(format);
        (void)api.createTexture(device, width, height, 1, 1, d3dFormat, 0, texture, 0);
        if (texture == 0) return {};
        (void)api.getSurfaceLevel(texture, 0, state.surface);
        if (state.surface == 0) return {false, false, texture};
        std::optional<DXSurfaceDescriptionForAnalysis> description;
        (void)api.getSurfaceDescription(state.surface, description);
        if (!description) return {false, false, texture};
        const auto size = CalculateBackingTextureSizeForAnalysis(*description);
        const auto attached = spDXTexture::AttachDeviceTextureForAnalysis(
            state.backing, width, height, spDXTexture::COMFormatToRuntimeForAnalysis(d3dFormat),
            texture, 0, size, api);
        if (!attached.completed)
            return {false, false, texture, attached.acquiredMipSurfaces};
        state.height = height;
        state.width = width;
        state.pixelFormat = pixelFormat;
        return {true, true, texture};
    }

    DXResourceResultForAnalysis spDXRenderTarget::ReinitializeDeviceTargetForAnalysis(
        DXRenderTargetStateForAnalysis& state, const DXResourceHandleForAnalysis device,
        const DXTextureResourceAPIForAnalysis& api)
    {
        // Native invalid-state branch emits a diagnostic then returns0. The
        // captured-state entry leaves that error/UI path explicitly incomplete.
        if (state.width == 0 || state.height == 0 || state.pixelFormat == 6) return {};
        if (!ReleaseDeviceTargetForAnalysis(state, api)) return {};
        return InitializeDeviceTargetForAnalysis(state, device,
            state.width, state.height, state.pixelFormat, api);
    }

    bool spDXCubeRenderTarget::ReleaseDeviceTargetForAnalysis(
        DXCubeRenderTargetStateForAnalysis& state,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.surfaces[0] == 0) return true; // Only face0 gates native body.
        // In the native domain every other face, backing texture and device is
        // valid here. Guard invalid captured cohorts before COM mutations.
        if (!api.release || state.backing.texture == 0 || state.backing.device == 0
            || std::any_of(state.surfaces.begin(), state.surfaces.end(),
                [](const auto surface) { return surface == 0; })) return false;
        (void)api.release(state.backing.texture);
        if (!spDXCubeTexture::ReleaseDeviceTextureForAnalysis(state.backing, api)) return false;
        for (auto& surface : state.surfaces)
        {
            (void)api.release(surface);
            surface = 0; // Ignores Release result, including nonzero.
        }
        return true;
    }

    DXResourceResultForAnalysis spDXCubeRenderTarget::InitializeDeviceTargetForAnalysis(
        DXCubeRenderTargetStateForAnalysis& state,
        const DXResourceHandleForAnalysis device,
        const std::uint32_t width, const std::uint32_t height,
        const std::uint32_t pixelFormat, DXTextureResourceGlobalsForAnalysis& globals,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (!ReleaseDeviceTargetForAnalysis(state, api)) return {};
        const auto format = static_cast<eTBPixelFormat>(pixelFormat);
        if (!IsSupportedFormatForAnalysis(format)) return {true, false, 0};
        if (device == 0 || !api.createCubeTexture || !api.getCubeMapSurface
            || !api.getSurfaceDescription || state.backing.palette != 0)
            return {};
        DXResourceHandleForAnalysis texture = 0;
        const auto d3dFormat = ToD3DFormatForAnalysis(format);
        // Cube device creation consumes width only; height is still published
        // to target and backing metadata later, even if it differs.
        (void)api.createCubeTexture(device, width, 1, 1, d3dFormat, 0, texture, 0);
        if (texture == 0) return {};
        for (std::uint32_t face = 0; face != 6; ++face)
        {
            (void)api.getCubeMapSurface(texture, face, 0, state.surfaces[face]);
            if (state.surfaces[face] == 0) return {false, false, texture};
            // Original remeasures face0 and reattaches the SAME texture for
            // each acquired face, rather than attaching once after the loop.
            std::optional<DXSurfaceDescriptionForAnalysis> description;
            (void)api.getSurfaceDescription(state.surfaces[0], description);
            if (!description) return {false, false, texture};
            const auto attached = spDXCubeTexture::AttachDeviceTextureForAnalysis(
                state.backing, width, height, d3dFormat, texture, 0,
                CalculateBackingTextureSizeForAnalysis(*description), globals, api);
            if (!attached.completed)
                return {false, false, texture, attached.acquiredMipSurfaces};
        }
        state.width = width;
        state.height = height;
        state.pixelFormat = pixelFormat;
        return {true, true, texture};
    }

    DXResourceResultForAnalysis spDXCubeRenderTarget::ReinitializeDeviceTargetForAnalysis(
        DXCubeRenderTargetStateForAnalysis& state, const DXResourceHandleForAnalysis device,
        DXTextureResourceGlobalsForAnalysis& globals,
        const DXTextureResourceAPIForAnalysis& api)
    {
        if (state.width == 0 || state.height == 0 || state.pixelFormat == 6) return {};
        if (!ReleaseDeviceTargetForAnalysis(state, api)) return {};
        return InitializeDeviceTargetForAnalysis(state, device,
            state.width, state.height, state.pixelFormat, globals, api);
    }
}
