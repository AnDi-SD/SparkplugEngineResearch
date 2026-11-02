#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxAIActionCommandExitHost.h"

namespace winx::reconstruction
{
    struct wxTrollAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        std::array<std::uint8_t, 164> rawOwn3AC;
    };
    class wxTrollAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6A421178;
        using CapturedState = wxTrollAttackAIActionCapturedStateForAnalysis;
        using ProfileForAnalysis = wxAIActionCommandExitProfileForAnalysis;
        using ResultForAnalysis = wxAIActionCommandExitResultForAnalysis;
        // Supplied postconstruction state and borrowed storage only. This does
        // not construct the native action/owner/actor or own their pointers.
        explicit wxTrollAttackAIAction(const CapturedState&,
            wxAIActionCommandExitHost* storage = nullptr,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxTrollAttackAIAction() override = default;
        wxTrollAttackAIAction(const wxTrollAttackAIAction&) = delete;
        wxTrollAttackAIAction& operator=(const wxTrollAttackAIAction&) = delete;
        wxTrollAttackAIAction(wxTrollAttackAIAction&&) = delete;
        wxTrollAttackAIAction& operator=(wxTrollAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        ResultForAnalysis ExitForAnalysis() noexcept;
        void vfunc_34_ClearForAnalysis() noexcept override; // PC primary cell2C.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const CapturedState& CapturedStateForAnalysis() const noexcept { return state_; }
        // Explicit between-call fixture input, not a recovered producer.
        CapturedState& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        CapturedState state_;
        wxAIActionCommandExitHost* const storage_; // borrowed, outlives action
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
    };
}
