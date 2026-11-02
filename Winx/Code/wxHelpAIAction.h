#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxHelpAIActionHost.h"
#include <array>

namespace winx::reconstruction
{
    struct wxHelpAIActionCapturedStateForAnalysis final
    {
        // Exact captured PC3A8..467 data, not portable native pointer objects.
        // Entry writes byte3AC and word460 only; other meanings remain open.
        std::array<std::uint8_t, 0xC0> ownBytes;
    };
    enum class wxHelpAIActionResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingActorOrCommand,
        UnqualifiedNotify, UnqualifiedHook,
    };
    class wxHelpAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x67435487;
        // Explicit postconstruction capture. This does not reconstruct the
        // native constructor or take ownership of captured/native pointers.
        wxHelpAIAction(wxHelpAIActionHost&, void* capturedOwner,
            const wxHelpAIActionCapturedStateForAnalysis&) noexcept;
        ~wxHelpAIAction() override = default;
        wxHelpAIAction(const wxHelpAIAction&) = delete;
        wxHelpAIAction& operator=(const wxHelpAIAction&) = delete;
        wxHelpAIAction(wxHelpAIAction&&) = delete;
        wxHelpAIAction& operator=(wxHelpAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxHelpAIActionResultForAnalysis EnterForAnalysis() noexcept;
        void vfunc_30() noexcept override; // PCslot28, void Entry5BAF70
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxHelpAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxHelpAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxHelpAIActionHost& host_;
        wxHelpAIActionCapturedStateForAnalysis state_;
        wxHelpAIActionResultForAnalysis lastResult_ = wxHelpAIActionResultForAnalysis::UnqualifiedProfile;
    };
}
