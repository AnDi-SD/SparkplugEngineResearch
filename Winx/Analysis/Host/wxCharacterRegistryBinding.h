#pragma once

namespace winx::reconstruction
{
    class wxCharacterRegistry;
    // Our storage-only view of an existing Registry binding. The record and
    // pointed-to captured Registry outlive each synchronous action operation.
    // Callers may replace the pointer between calls; it is stable within a call.
    // This does not reconstruct the native global producer or singleton lifetime.
    struct wxCharacterRegistryBindingForAnalysis final
    {
        wxCharacterRegistry* existing = nullptr;
    };
}
