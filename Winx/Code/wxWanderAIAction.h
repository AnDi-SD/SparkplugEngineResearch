#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxWanderAIActionHost.h"

namespace winx::reconstruction
{
    struct wxWanderCapturedStateForAnalysis final
    {
        // Native3A8..40F represented only as captured raw words. Opaque cells,
        // including native pointer-shaped words, are never dereferenced or owned.
        std::array<std::uint32_t, 26> nativeTailWords3A8;
    };
    enum class wxWanderResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingEntity, MissingNode,
        UnqualifiedNodeType, UnqualifiedPosition, UnqualifiedNotify, UnqualifiedHook,
    };
    class wxWanderAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x5E7865EA;
        // Explicit postconstruction capture; not a native constructor/lifetime
        // replacement. The inherited action graph must remain empty for Entry.
        wxWanderAIAction(wxWanderAIActionHost&, void* capturedOwner,
            const wxWanderCapturedStateForAnalysis&) noexcept;
        ~wxWanderAIAction() override = default;
        wxWanderAIAction(const wxWanderAIAction&) = delete;
        wxWanderAIAction& operator=(const wxWanderAIAction&) = delete;
        wxWanderAIAction(wxWanderAIAction&&) = delete;
        wxWanderAIAction& operator=(wxWanderAIAction&&) = delete;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxWanderResultForAnalysis EnterForAnalysis() noexcept;
        void vfunc_30() noexcept override; // PCslot28, void Entry5AB420, unused argument.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        [[nodiscard]] const wxWanderCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        [[nodiscard]] wxWanderResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxWanderAIActionHost& host_;
        wxWanderCapturedStateForAnalysis state_;
        wxWanderResultForAnalysis lastResult_ = wxWanderResultForAnalysis::UnqualifiedProfile;
    };
}
