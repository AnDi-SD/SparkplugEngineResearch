#pragma once
#include "wxMenuGameFlowState.h"
#include "Analysis/Host/wxPressStartGameFlowStateHost.h"
#include "Code/Sparkplug/spTimer.h"
#include <optional>

namespace winx::reconstruction
{
    // Inferred portable API, not native8C storage. Physical Menu prefix0,
    // PC construction/lifetime, own Tick, admitted empty-graph Entry and
    // separately admitted synchronous Exit are represented.
    class wxPressStartGameFlowState final : public wxMenuGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0xAB39C152;
        struct PressStartStateForAnalysis final
        {
            void* borrowedInput5C=nullptr;
            std::uint8_t field60=0;
            // Native64 and padding61..63 are unwritten by construction.
            // A supplied capture is fixture state, never a native default.
            // Admitted Entry later publishes known0 before its timer calls.
            std::optional<std::uint32_t> capturedWord64;
        };
        explicit wxPressStartGameFlowState(wxPressStartGameFlowStateHost&);
        ~wxPressStartGameFlowState() override;
        static void SetFactoryHostForAnalysis(wxPressStartGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Copy/Notify remain literal inherited no-ops. Clone constructs fresh
        // timers/input/defaults instead of copying the source's own state.
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        bool FlowSlot30ForAnalysis() override;
        [[nodiscard]] const PressStartStateForAnalysis& GetPressStartStateForAnalysis() const noexcept {return pressStart_;}
        PressStartStateForAnalysis& PressStartStateForFixture() noexcept {return pressStart_;}
        [[nodiscard]] const sparkplug::reconstruction::spTimer& EmbeddedTimerForAnalysis() const noexcept {return embeddedTimer_;}
        sparkplug::reconstruction::spTimer& EmbeddedTimerForFixture() noexcept {return embeddedTimer_;}
    private:
        wxPressStartGameFlowStateHost& pressStartHost_;
        PressStartStateForAnalysis pressStart_;
        sparkplug::reconstruction::spTimer embeddedTimer_;
    };
}
