#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxCharacterRegistryBinding.h"
#include "Analysis/Host/wxDarcyAttackAIActionExitHost.h"

namespace winx::reconstruction
{
    struct wxDarcyAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        // Captured PC3AC..45F data. Encoded pointers stay opaque bytes;
        // this facade neither casts nor owns them.
        std::array<std::uint8_t, 0xB4> ownBytes3AC;
    };
    enum class wxDarcyAttackAIActionResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingExistingRegistry,
        RegistryNotAdmitted, UnqualifiedNotify, UnqualifiedHook,
    };
    enum class wxDarcyAttackAIActionExitResultForAnalysis
    { Completed, UnqualifiedProfile, MissingOwner, MissingActorStorage, MissingCommandStorage };
    class wxDarcyAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x590C5D10;
        enum class ProfileForAnalysis { Unqualified, PcCapturedExistingRegistryEntry };
        // Host postconstruction binding, not a recovered native constructor.
        // The shared binding record is borrowed and read freshly per Entry.
        wxDarcyAttackAIAction(wxCharacterRegistryBindingForAnalysis&,
            const wxDarcyAttackAIActionCapturedStateForAnalysis&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxDarcyAttackAIAction() override = default;
        wxDarcyAttackAIAction(const wxDarcyAttackAIAction&) = delete;
        wxDarcyAttackAIAction& operator=(const wxDarcyAttackAIAction&) = delete;
        wxDarcyAttackAIAction(wxDarcyAttackAIAction&&) = delete;
        wxDarcyAttackAIAction& operator=(wxDarcyAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxDarcyAttackAIActionResultForAnalysis EnterForAnalysis() noexcept;
        // Separate default-unbound Exit admission. Neither binding nor
        // borrowed records are owned; the host outlives this action.
        void BindExitForAnalysis(wxDarcyAttackAIActionExitHost* host) noexcept { exitHost_ = host; }
        wxDarcyAttackAIActionExitResultForAnalysis ExitForAnalysis() noexcept;
        wxDarcyAttackAIActionExitResultForAnalysis LastExitResultForAnalysis() const noexcept { return lastExitResult_; }
        void vfunc_30() noexcept override; // PCslot28, Entry5C23D0; argument unused.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxDarcyAttackAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxDarcyAttackAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxCharacterRegistryBindingForAnalysis& binding_;
        ProfileForAnalysis profile_;
        wxDarcyAttackAIActionCapturedStateForAnalysis state_;
        wxDarcyAttackAIActionResultForAnalysis lastResult_ = wxDarcyAttackAIActionResultForAnalysis::UnqualifiedProfile;
        wxDarcyAttackAIActionExitHost* exitHost_ = nullptr;
        wxDarcyAttackAIActionExitResultForAnalysis lastExitResult_ = wxDarcyAttackAIActionExitResultForAnalysis::UnqualifiedProfile;
    };
}
