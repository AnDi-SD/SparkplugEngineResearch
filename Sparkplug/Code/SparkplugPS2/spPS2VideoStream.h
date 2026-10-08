#pragma once

// Registered PS2 class. Portable API names/layout are our analytical facade.
// Each interface leaf below is a complete body from the shipped PS2 game.
#include "../Sparkplug/spVideoStream.h"

namespace sparkplug::reconstruction
{
    class spPS2VideoStream final : public spVideoStream
    {
    public:
        static constexpr spClassID ClassID = 0x15747650;
        spPS2VideoStream() = default;
        ~spPS2VideoStream() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        // Offsets are relative to the first function in the PS2 primary
        // interface (+8 after the GCC table prefix). Argument types/names are
        // unknown; these complete bodies do not read their arguments.
        [[nodiscard]] bool NativeHook00ForAnalysis() const noexcept { return true; }
        void NativeHook04ForAnalysis() const noexcept {}
        [[nodiscard]] bool NativeHook08ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook0CForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook10ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook14ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook18ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook1CForAnalysis() const noexcept { return true; }
        [[nodiscard]] std::uint32_t NativeHook20ForAnalysis() const noexcept { return 0; }
        [[nodiscard]] std::uint32_t NativeHook24ForAnalysis() const noexcept { return 0; }
    };
}
