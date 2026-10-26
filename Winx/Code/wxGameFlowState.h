#pragma once

// Inferred portable API, not the original 32-bit class ABI. Constructor/lifetime
// scope is PC; PS2 constructor reads additional globals and is not modeled here.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxGameFlowStateHost.h"

namespace winx::reconstruction
{
    class wxGameFlowState : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x11521AFA;
        static constexpr std::uint32_t UnchangedCode = 0xFFFFFFFF;
        struct StateForAnalysis final
        {
            std::uint32_t field10 = 0;
            std::uint8_t field14 = 0;
            std::array<std::uint32_t, 3> codes{UnchangedCode, UnchangedCode, UnchangedCode};
            std::uint8_t field24 = 1, field25 = 1;
            std::uint32_t field34 = 0;
            std::uint8_t field38 = 1;
            // Padding and native word28 are unwritten by the constructor.
        };

        explicit wxGameFlowState(wxGameFlowStateHost&);
        ~wxGameFlowState() override;
        wxGameFlowState(const wxGameFlowState&) = delete;
        wxGameFlowState& operator=(const wxGameFlowState&) = delete;
        static void SetFactoryHostForAnalysis(wxGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Copy is the inherited literal spBaseObject no-op: own state, timer
        // and borrowed input are not transferred to an existing destination.

        void SetCodesForAnalysis(std::uint32_t, std::uint32_t, std::uint32_t) noexcept;
        void GetCodesForAnalysis(std::uint32_t&, std::uint32_t&, std::uint32_t&) const noexcept;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        StateForAnalysis& MutableStateForAnalysis() noexcept { return state_; }
        StateForAnalysis& StateForFixture() noexcept { return state_; }
        [[nodiscard]] void* GetBorrowedInputForAnalysis() const noexcept { return input_; }
        [[nodiscard]] sparkplug::reconstruction::spTaskTimer& TimerForAnalysis() noexcept { return *timer_; }
        [[nodiscard]] const sparkplug::reconstruction::spTaskTimer& TimerForAnalysis() const noexcept { return *timer_; }

        // Six actual base hooks, named by PC native slot instead of guessing
        // menu lifecycle meanings. PS2 slots are shifted by its table header.
        virtual bool FlowSlot1CForAnalysis() { return true; }
        virtual void FlowSlot20ForAnalysis() {}
        virtual void FlowSlot24ForAnalysis() {}
        virtual void FlowSlot28ForAnalysis() {}
        virtual void FlowSlot2CForAnalysis() {}
        virtual bool FlowSlot30ForAnalysis() { return true; }

        static void ResetResourcesForAnalysis(wxGameFlowResourcesForAnalysis&) noexcept;
        [[nodiscard]] static std::uint32_t RemapResourceCodeForAnalysis(
            std::uint32_t code, std::uint32_t lowerState, std::int32_t profile514) noexcept;
    private:
        wxGameFlowStateHost& host_;
        StateForAnalysis state_{};
        void* input_ = nullptr; // borrowed; destructor never releases it
        std::unique_ptr<sparkplug::reconstruction::spTaskTimer> timer_;
    };
}
