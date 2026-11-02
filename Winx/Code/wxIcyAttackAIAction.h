#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxCharacterRegistryBinding.h"
#include "Analysis/Host/wxIcyAttackAIActionExitHost.h"

namespace winx::reconstruction
{
    struct wxIcyAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        // Captured PC3AC..47F data. Encoded pointers stay opaque bytes;
        // this facade neither casts nor owns them.
        std::array<std::uint8_t, 0xD4> ownBytes3AC;
    };
    enum class wxIcyAttackAIActionResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingExistingRegistry,
        RegistryNotAdmitted, UnqualifiedNotify, UnqualifiedHook,
    };
    enum class wxIcyAttackAIActionExitResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingActorStorage,
        MissingCommandStorage, CommandNotAdmitted, MissingExistingProfile,
    };
    class wxIcyAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x510661EC;
        enum class ProfileForAnalysis { Unqualified, PcCapturedExistingRegistryEntry };
        // Host postconstruction binding, not a recovered native constructor.
        // The shared binding record is borrowed and read freshly per Entry.
        wxIcyAttackAIAction(wxCharacterRegistryBindingForAnalysis&,
            const wxIcyAttackAIActionCapturedStateForAnalysis&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxIcyAttackAIAction() override = default;
        wxIcyAttackAIAction(const wxIcyAttackAIAction&) = delete;
        wxIcyAttackAIAction& operator=(const wxIcyAttackAIAction&) = delete;
        wxIcyAttackAIAction(wxIcyAttackAIAction&&) = delete;
        wxIcyAttackAIAction& operator=(wxIcyAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxIcyAttackAIActionResultForAnalysis EnterForAnalysis() noexcept;
        // Separate default-unbound Exit admission. Host and all supplied
        // command/profile storage outlive this facade and are never owned.
        void BindExitForAnalysis(wxIcyAttackAIActionExitHost* host) noexcept { exitHost_ = host; }
        wxIcyAttackAIActionExitResultForAnalysis ExitForAnalysis() noexcept;
        wxIcyAttackAIActionExitResultForAnalysis LastExitResultForAnalysis() const noexcept { return lastExitResult_; }
        void vfunc_30() noexcept override; // PCslot28, Entry5BE230; argument unused.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxIcyAttackAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxIcyAttackAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxCharacterRegistryBindingForAnalysis& binding_;
        ProfileForAnalysis profile_;
        wxIcyAttackAIActionCapturedStateForAnalysis state_;
        wxIcyAttackAIActionResultForAnalysis lastResult_ = wxIcyAttackAIActionResultForAnalysis::UnqualifiedProfile;
        wxIcyAttackAIActionExitHost* exitHost_ = nullptr;
        wxIcyAttackAIActionExitResultForAnalysis lastExitResult_ = wxIcyAttackAIActionExitResultForAnalysis::UnqualifiedProfile;
    };
}
