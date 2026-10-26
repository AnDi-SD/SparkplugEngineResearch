#pragma once

#include "wxGameFlowState.h"
#include "Analysis/Host/wxDialogWindowGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Inferred portable API. Native PC/PS2 derived extents are3C with no own
    // tail fields. Parent construction/lifetime below are qualified for PC;
    // the own window clear/consumer sequence is also known for PS2.
    class wxDialogWindowGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x4D6F71CC;
        explicit wxDialogWindowGameFlowState(wxDialogWindowGameFlowStateHost&);
        static void SetFactoryHostForAnalysis(wxDialogWindowGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        void FlowSlot24ForAnalysis() override;
        bool FlowSlot30ForAnalysis() override;
    private:
        wxDialogWindowGameFlowStateHost& hookHost_;
    };
}
