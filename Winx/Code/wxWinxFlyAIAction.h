#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxWinxFlyAIActionHost.h"

namespace winx::reconstruction
{
    struct wxWinxFlyAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        // Native3AC..3BF raw data. Entry clears five destinations; the last
        // three padding bytes are preserved. No native pointer is cast/owned.
        std::array<std::uint8_t, 0x14> ownBytes3AC;
    };
    enum class wxWinxFlyAIActionResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingActorOrCommandStorage,
        MissingExistingRegistry, RegistryNotAdmitted, UnqualifiedNotify, UnqualifiedHook,
    };
    class wxWinxFlyAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x67AF3520;
        // Explicit postconstruction capture with an empty inherited action graph.
        // No native constructor, PathFinder or owning lifetime equivalence.
        wxWinxFlyAIAction(wxWinxFlyAIActionHost&, void* capturedOwner,
            const wxWinxFlyAIActionCapturedStateForAnalysis&) noexcept;
        ~wxWinxFlyAIAction() override = default;
        wxWinxFlyAIAction(const wxWinxFlyAIAction&) = delete;
        wxWinxFlyAIAction& operator=(const wxWinxFlyAIAction&) = delete;
        wxWinxFlyAIAction(wxWinxFlyAIAction&&) = delete;
        wxWinxFlyAIAction& operator=(wxWinxFlyAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxWinxFlyAIActionResultForAnalysis EnterForAnalysis() noexcept;
        void vfunc_30() noexcept override; // PCslot28, void Entry5BC6A0; argument unused.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxWinxFlyAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxWinxFlyAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxWinxFlyAIActionHost& host_;
        wxWinxFlyAIActionCapturedStateForAnalysis state_;
        wxWinxFlyAIActionResultForAnalysis lastResult_ = wxWinxFlyAIActionResultForAnalysis::UnqualifiedProfile;
    };
}
