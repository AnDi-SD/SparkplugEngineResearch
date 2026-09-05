#pragma once

// Inferred header path.  Both shipped binaries preserve this class name and
// its position in the RTTI graph, but no original header/source path survives.
// Method names and callback typedef below describe observed behavior and are
// not claimed as original spellings.

#include "spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spMemoryStream;

    class spAsyncFileStreamManager : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x7EA51364;
        using CompletionCallback = void (*)(void* context);

        spAsyncFileStreamManager() noexcept;
        ~spAsyncFileStreamManager() override;

        spAsyncFileStreamManager(const spAsyncFileStreamManager&) = delete;
        spAsyncFileStreamManager& operator=(
            const spAsyncFileStreamManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spAsyncFileStreamManager* GetInstance() noexcept;

        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC callers and both platform leaves prove this four-argument
        // contract.  Both leaf request implementations complete synchronously
        // in the shipped builds; the PS2 object additionally retains a large
        // queue/update state whose producer is not present in known code.  The
        // original method name is not present in either executable.
        [[nodiscard]] virtual bool vfunc_Request(
            const char* streamName,
            spMemoryStream* destination,
            CompletionCallback completion,
            void* context) = 0;

        // Immediate no-op in the common PC class.  The PS2 leaf uses this slot
        // to advance queued work; original spelling is unknown.
        virtual void vfunc_Update() noexcept;

    private:
        // Portable representation of the process-wide pointer installed by
        // native constructor paths.  The separate native dispatch subobject at
        // +0x14 remains in the ABI evidence layouts rather than being faked in
        // this host object.
        static spAsyncFileStreamManager* instance_;
    };
}
