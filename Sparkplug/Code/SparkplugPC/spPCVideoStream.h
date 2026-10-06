#pragma once

// PC class is a native dormant interface: its actual table returns true,
// zero and no-op, with one wrapper that invokes the no-op and returns AL1.
// These are original leaf bodies, not substitutes for unknown video code.
#include "../Sparkplug/spVideoStream.h"

namespace sparkplug::reconstruction
{
    class spPCVideoStream final : public spVideoStream
    {
    public:
        static constexpr spClassID ClassID = 0x1DC67471;
        spPCVideoStream() = default;
        ~spPCVideoStream() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Opaque aliases for proved primary interface slots. Original API names
        // and argument types are unknown; leaf bodies never read arguments.
        [[nodiscard]] bool NativeHook00ForAnalysis() noexcept;
        void NativeHook04ForAnalysis() noexcept {}
        [[nodiscard]] bool NativeHook08ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook0CForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook10ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook14ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook18ForAnalysis() const noexcept { return true; }
        [[nodiscard]] bool NativeHook1CForAnalysis() const noexcept { return true; }
        [[nodiscard]] std::uint32_t NativeHook20ForAnalysis() const noexcept { return 0; }
        [[nodiscard]] std::uint32_t NativeHook24ForAnalysis() const noexcept { return 0; }
        void NativeHook28ForAnalysis() noexcept {}
    };
}
