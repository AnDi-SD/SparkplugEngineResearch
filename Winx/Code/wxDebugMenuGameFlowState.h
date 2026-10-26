#pragma once

#include "wxGameFlowState.h"
#include "Analysis/Host/wxDebugMenuGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Portable declaration; native own fields are absent after the3C parent.
    // Parent construction/lifetime use PC semantics. PS2 selects only the
    // recovered differences in these derived hooks, not a PS2 constructor.
    class wxDebugMenuGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x38DE5A48;
        enum class HookPlatformForAnalysis { PC, PS2 };
        explicit wxDebugMenuGameFlowState(wxDebugMenuGameFlowStateHost&,
            HookPlatformForAnalysis = HookPlatformForAnalysis::PC);
        static void SetFactoryHostForAnalysis(wxDebugMenuGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        [[nodiscard]] HookPlatformForAnalysis GetHookPlatformForAnalysis() const noexcept { return platform_; }
    private:
        wxDebugMenuGameFlowStateHost& hookHost_;
        HookPlatformForAnalysis platform_;
    };
}
