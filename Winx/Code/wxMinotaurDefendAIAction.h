#pragma once

#include "Code/wxAIAction.h"
#include "Code/wxPerception.h"
#include "Analysis/Host/wxMinotaurDefendAIActionHost.h"

namespace winx::reconstruction
{
    enum class wxMinotaurDefendPlatformForAnalysis { PC, PS2 };
    enum class wxMinotaurDefendGateResultForAnalysis
    {
        NotRun,
        ExpiredSelector,
        StateReset,
        UnqualifiedState0,
        UnqualifiedState1,
    };

    class wxMinotaurDefendAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x2F567161;
        explicit wxMinotaurDefendAIAction(wxMinotaurDefendAIActionHost&,
            wxMinotaurDefendPlatformForAnalysis = wxMinotaurDefendPlatformForAnalysis::PC) noexcept;
        static void SetFactoryHostForAnalysis(wxMinotaurDefendAIActionHost*,
            wxMinotaurDefendPlatformForAnalysis = wxMinotaurDefendPlatformForAnalysis::PC) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;

        void vfunc_0C(const void*) noexcept override;
        // Unknown owned state bodies are not invoked through successful host
        // stubs. False here is OUR adapter refusal, not a recovered native false;
        // inspect LastGateResult or call the explicit bounded gate instead.
        bool vfunc_24() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        [[nodiscard]] wxMinotaurDefendGateResultForAnalysis UpdateGateForAnalysis() noexcept;
        wxMinotaurDefendGateResultForAnalysis LastGateResultForAnalysis() const noexcept { return lastGate_; }

        void* TargetForAnalysis() const noexcept { return target_; }
        std::uint32_t StateForAnalysis() const noexcept { return state_; }
        std::uint32_t DeadlineForAnalysis() const noexcept { return deadline_; }
        void* CachedCommandForAnalysis() const noexcept { return command_; }
        void SetFieldsForAnalysis(void* target, std::uint32_t state,
            std::uint32_t deadline, void* command) noexcept
        { target_ = target; state_ = state; deadline_ = deadline; command_ = command; }

    private:
        wxPerceptionNumericProfileForAnalysis PerceptionProfileForAnalysis() const noexcept;
        wxMinotaurDefendAIActionHost& host_;
        wxMinotaurDefendPlatformForAnalysis platform_;
        void* target_ = nullptr; // borrowed raw Copy, no AddRef/Delete
        std::uint32_t state_ = 1;
        std::uint32_t deadline_ = 0;
        void* command_ = nullptr; // borrowed cached command
        wxMinotaurDefendGateResultForAnalysis lastGate_ = wxMinotaurDefendGateResultForAnalysis::NotRun;
    };
}
