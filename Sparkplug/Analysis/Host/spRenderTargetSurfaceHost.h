#pragma once

// HOST resource-description boundary. This is not an original engine class,
// D3D COM declaration, or reconstructed native object layout. Providers must
// supply initialized fields from the currently acquired ordinary surface or
// cube face zero. Their context remains borrowed for the duration of the call.
#include <cstdint>

namespace sparkplug::analysis::host
{
    struct RenderTargetSurfaceDescription final
    {
        std::uint32_t format;
        std::uint32_t width;
        std::uint32_t height;
    };

    // false is a HOST refusal to expose uninitialized GetDesc output. Native
    // helpers ignore HRESULT and have no defined result for unwritten fields.
    // A provider may return true after a failed HRESULT if it still supplies
    // valid fields; the original arithmetic is independent of that HRESULT.
    using RenderTargetSurfaceDescriptionReader = bool (*)(
        void* context, RenderTargetSurfaceDescription& description);
}
