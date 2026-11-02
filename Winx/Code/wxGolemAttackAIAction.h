#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxAIActionCommandExitHost.h"
#include "Analysis/Host/wxGolemAttackAIActionEntryBindings.h"

namespace winx::reconstruction
{
    struct wxGolemAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        std::array<std::uint8_t, 20> ownBytes3AC;
    };
    class wxGolemAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x4A405186;
        static constexpr std::size_t RawOwnBytesForAnalysis = 20;
        using CapturedState = wxGolemAttackAIActionCapturedStateForAnalysis;
        using ProfileForAnalysis = wxAIActionCommandExitProfileForAnalysis;
        using ResultForAnalysis = wxAIActionCommandExitResultForAnalysis;
        // Explicit postconstruction data binding; no native constructor,
        // old encoded-pointer ownership or nonempty action graph is recovered.
        wxGolemAttackAIAction(wxAIActionCommandExitHost&, void* capturedOwner,
            const CapturedState&, ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxGolemAttackAIAction() override = default;
        wxGolemAttackAIAction(const wxGolemAttackAIAction&) = delete;
        wxGolemAttackAIAction& operator=(const wxGolemAttackAIAction&) = delete;
        wxGolemAttackAIAction(wxGolemAttackAIAction&&) = delete;
        wxGolemAttackAIAction& operator=(wxGolemAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        ResultForAnalysis ExitForAnalysis() noexcept;
        // Independent captured Entry admission; the existing Exit profile
        // does not qualify the Registry/owner inputs for this operation.
        void BindEntryForAnalysis(wxGolemAttackAIActionEntryBindingsForAnalysis* binding) noexcept
        { entryBinding_ = binding; }
        wxGolemAttackAIActionEntryResultForAnalysis EnterForAnalysis() noexcept;
        wxGolemAttackAIActionEntryResultForAnalysis LastEntryResultForAnalysis() const noexcept
        { return lastEntryResult_; }
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
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxAIActionCommandExitHost& host_;
        CapturedState state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        wxGolemAttackAIActionEntryBindingsForAnalysis* entryBinding_ = nullptr;
        wxGolemAttackAIActionEntryResultForAnalysis lastEntryResult_ =
            wxGolemAttackAIActionEntryResultForAnalysis::UnqualifiedProfile;
    };
}
