#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxGhoulScriptAIActionHost.h"

namespace winx::reconstruction
{
    struct wxGhoulScriptCapturedStateForAnalysis final
    {
        std::uint32_t state3A8;
        std::uint32_t word3AC;
        sparkplug::reconstruction::spNode* borrowedNode3B0;
        std::uint32_t opaqueNativeWord3B4;
        void* borrowedCommand3B8;
    };
    enum class wxGhoulScriptResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingEntity,
        MissingActor, UnqualifiedNodeType, UnqualifiedNotify, UnqualifiedHook,
    };
    class wxGhoulScriptAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x75E255E9;
        // Explicit postconstruction capture, not the whole native constructor.
        // All pointers are borrowed live C++ identities, never raw native32 casts.
        wxGhoulScriptAIAction(wxGhoulScriptAIActionHost&, void* capturedOwner,
            const wxGhoulScriptCapturedStateForAnalysis&) noexcept;
        ~wxGhoulScriptAIAction() override = default;
        wxGhoulScriptAIAction(const wxGhoulScriptAIAction&) = delete;
        wxGhoulScriptAIAction& operator=(const wxGhoulScriptAIAction&) = delete;
        wxGhoulScriptAIAction(wxGhoulScriptAIAction&&) = delete;
        wxGhoulScriptAIAction& operator=(wxGhoulScriptAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxGhoulScriptResultForAnalysis EnterForAnalysis() noexcept;
        void vfunc_30() noexcept override; // PCslot28, void Entry5A9930
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxGhoulScriptCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxGhoulScriptResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxGhoulScriptAIActionHost& host_;
        wxGhoulScriptCapturedStateForAnalysis state_;
        wxGhoulScriptResultForAnalysis lastResult_ = wxGhoulScriptResultForAnalysis::UnqualifiedProfile;
    };
}
