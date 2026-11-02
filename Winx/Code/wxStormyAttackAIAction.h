#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxCharacterRegistryBinding.h"
#include "Analysis/Host/wxStormyAttackAIActionExitHost.h"
#include <cstddef>

namespace winx::reconstruction
{
    struct wxStormyAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        // Complete native3AC..46F captured input. Unknown cells and padding
        // remain raw; encoded native pointers are never cast or owned here.
        std::array<std::uint8_t, 0xC4> ownBytes3AC;
    };
    enum class wxStormyAttackAIActionProfileForAnalysis
    { Unqualified, PcExistingCapturedRegistry };
    enum class wxStormyAttackAIActionResultForAnalysis
    { Completed, UnqualifiedProfile, MissingExistingRegistry, RegistryNotAdmitted, UnqualifiedNotify, UnqualifiedHook };
    enum class wxStormyAttackAIActionExitResultForAnalysis
    { Completed, UnqualifiedProfile, MissingOwner, MissingActorStorage, MissingCommandStorage, CommandNotAdmitted };

    class wxStormyAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x32957C4C;
        // Explicit postconstruction state with an empty inherited action graph.
        // The binding, Registry and its borrowed records outlive this action.
        wxStormyAttackAIAction(wxCharacterRegistryBindingForAnalysis&, void* capturedOwner,
            const wxStormyAttackAIActionCapturedStateForAnalysis&,
            wxStormyAttackAIActionProfileForAnalysis = wxStormyAttackAIActionProfileForAnalysis::Unqualified) noexcept;
        ~wxStormyAttackAIAction() override = default;
        wxStormyAttackAIAction(const wxStormyAttackAIAction&) = delete;
        wxStormyAttackAIAction& operator=(const wxStormyAttackAIAction&) = delete;
        wxStormyAttackAIAction(wxStormyAttackAIAction&&) = delete;
        wxStormyAttackAIAction& operator=(wxStormyAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxStormyAttackAIActionResultForAnalysis EnterForAnalysis() noexcept;
        // Separate default-unbound Exit profile; all records/host stay live.
        void BindExitForAnalysis(wxStormyAttackAIActionExitHost* host) noexcept { exitHost_ = host; }
        wxStormyAttackAIActionExitResultForAnalysis ExitForAnalysis() noexcept;
        wxStormyAttackAIActionExitResultForAnalysis LastExitResultForAnalysis() const noexcept { return lastExitResult_; }
        void vfunc_30() noexcept override; // PCslot28, void Entry5C56F0; argument unused.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxStormyAttackAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxStormyAttackAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        void WriteOwnWordForAnalysis(std::size_t nativeOffset, std::uint32_t value) noexcept;
        bool RefuseHookForAnalysis() noexcept;
        wxCharacterRegistryBindingForAnalysis& registryBinding_;
        wxStormyAttackAIActionCapturedStateForAnalysis state_;
        const wxStormyAttackAIActionProfileForAnalysis profile_;
        wxStormyAttackAIActionResultForAnalysis lastResult_ = wxStormyAttackAIActionResultForAnalysis::UnqualifiedProfile;
        wxStormyAttackAIActionExitHost* exitHost_ = nullptr;
        wxStormyAttackAIActionExitResultForAnalysis lastExitResult_ = wxStormyAttackAIActionExitResultForAnalysis::UnqualifiedProfile;
    };
}
