#pragma once

// Original PS2 class identity; path inferred. This concrete platform leaf has
// no storage beyond spCubeTexture; its four spITexture hooks are native stubs.
#include "../Sparkplug/spCubeTexture.h"

namespace sparkplug::reconstruction
{
    class spPS2CubeTexture final : public spCubeTexture
    {
    public:
        static constexpr spClassID ClassID = 0x18062B6E;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        // Analytical facades for001F35B0/C0/D0/E0. Native signatures remain
        // unknown; the bodies ignore every argument and write no object state.
        [[nodiscard]] static constexpr bool NativeHook35B0ForAnalysis() noexcept { return false; }
        [[nodiscard]] static constexpr bool NativeHook35C0ForAnalysis() noexcept { return false; }
        static constexpr void NativeHook35D0ForAnalysis() noexcept {}
        [[nodiscard]] static constexpr bool NativeHook35E0ForAnalysis() noexcept { return true; }

        // State-only facade of00173770: first face dimensions, u32 argument
        // stored at18, flags20 and normalization boolean. It calls the native
        // primary48 success stub and does not create texture storage.
        [[nodiscard]] bool InitCubeBufferStateForAnalysis(std::uint32_t width, std::uint32_t height,
            std::uint32_t field18, std::uint32_t flags, bool normalize) noexcept;
        [[nodiscard]] static constexpr bool HasBackendCubeForAnalysis() noexcept { return false; }
    };
}
