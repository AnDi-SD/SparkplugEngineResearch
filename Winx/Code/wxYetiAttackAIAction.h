#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxAIActionCommandExitHost.h"
#include "Analysis/Host/wxYetiAttackAIActionPhaseBindings.h"

namespace winx::reconstruction
{
    struct wxYetiAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        std::array<std::uint8_t, 232> ownBytes3AC;
    };
    class wxYetiAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x30347478;
        static constexpr std::size_t RawOwnBytesForAnalysis = 232;
        using CapturedState = wxYetiAttackAIActionCapturedStateForAnalysis;
        using ProfileForAnalysis = wxAIActionCommandExitProfileForAnalysis;
        using ResultForAnalysis = wxAIActionCommandExitResultForAnalysis;
        wxYetiAttackAIAction(wxAIActionCommandExitHost&, void* capturedOwner,
            const CapturedState&, ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxYetiAttackAIAction() override = default;
        wxYetiAttackAIAction(const wxYetiAttackAIAction&) = delete;
        wxYetiAttackAIAction& operator=(const wxYetiAttackAIAction&) = delete;
        wxYetiAttackAIAction(wxYetiAttackAIAction&&) = delete;
        wxYetiAttackAIAction& operator=(wxYetiAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        ResultForAnalysis ExitForAnalysis() noexcept;
        // Independent captured method selected by PC primary54. This named
        // analytical API does not impose that slot on the portable vtable.
        void BindPhase54ForAnalysis(wxYetiAttackAIActionPhaseBindingsForAnalysis* binding) noexcept
        { phaseBinding_ = binding; }
        wxYetiAttackAIActionPhaseResultForAnalysis Phase54ForAnalysis() noexcept;
        wxYetiAttackAIActionPhaseResultForAnalysis LastPhase54ResultForAnalysis() const noexcept
        { return lastPhaseResult_; }
        void vfunc_34_ClearForAnalysis() noexcept override; // PCslot2C selects common5B79D0.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const CapturedState& CapturedStateForAnalysis() const noexcept { return state_; }
        // Explicit between-call captured input control, not a native setter.
        CapturedState& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxAIActionCommandExitHost& host_;
        CapturedState state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        wxYetiAttackAIActionPhaseBindingsForAnalysis* phaseBinding_ = nullptr;
        wxYetiAttackAIActionPhaseResultForAnalysis lastPhaseResult_ =
            wxYetiAttackAIActionPhaseResultForAnalysis::UnqualifiedProfile;
    };
}
