#pragma once
#include "wxMenuGameFlowState.h"
#include "Analysis/Host/wxGameOverGameFlowStateHost.h"

namespace winx::reconstruction
{
    // PC physical Menu prefix plus one borrowed input pointer. Portable state
    // and API spellings do not reproduce the32-bit native C++ layout.
    class wxGameOverGameFlowState final : public wxMenuGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x331018ED;
        struct StateForAnalysis final {void* borrowedInput5C=nullptr;};
        explicit wxGameOverGameFlowState(wxGameOverGameFlowStateHost&);
        ~wxGameOverGameFlowState() override=default;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Existing literal common Copy does not transfer5C or parent state.
        bool FlowSlot30ForAnalysis() override;
        void FlowSlot20ForAnalysis() override;
        // Inherited Menu Exit under a separate explicit borrowed-binding profile.
        void FlowSlot24ForAnalysis() override;
        void FlowSlot28ForAnalysis() override;
        void FlowSlot2CForAnalysis() override;
        // Extended38/40 remain unqualified. The measured44 target forwards
        // to the real parent only under the explicit empty Entry profile.
        [[nodiscard]] bool MenuSlot38ForAnalysis() override;
        void MenuSlot40ForAnalysis(const char*) override;
        [[nodiscard]] bool MenuSlot44ForAnalysis() override;
        [[nodiscard]] const StateForAnalysis& GetGameOverStateForAnalysis() const noexcept{return gameOver_;}
        StateForAnalysis& GameOverStateForFixture() noexcept{return gameOver_;}
    private:
        void RequireEntryProfileForAnalysis() const;
        wxGameOverGameFlowStateHost& gameOverHost_;
        StateForAnalysis gameOver_;
    };
}
