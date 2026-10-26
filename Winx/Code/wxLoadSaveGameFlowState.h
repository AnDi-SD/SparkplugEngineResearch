#pragma once

#include "wxGameFlowState.h"
#include "Analysis/Host/wxLoadSaveGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Inferred API. Both native layouts add no fields to the 3C-byte parent.
    // Construction/lifetime below reuse the PC parent reconstruction. The
    // PS2 selector qualifies the derived hooks only, not the PS2 parent ctor.
    class wxLoadSaveGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x612C1BB8;
        enum class HookPlatformForAnalysis { PC, PS2 };

        explicit wxLoadSaveGameFlowState(wxLoadSaveGameFlowStateHost&,
            HookPlatformForAnalysis platform = HookPlatformForAnalysis::PC);
        static void SetFactoryHostForAnalysis(wxLoadSaveGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;

        void FlowSlot24ForAnalysis() override;
        bool FlowSlot30ForAnalysis() override;
        [[nodiscard]] HookPlatformForAnalysis GetHookPlatformForAnalysis() const noexcept { return platform_; }

    private:
        wxLoadSaveGameFlowStateHost& hookHost_;
        HookPlatformForAnalysis platform_;
    };
}
