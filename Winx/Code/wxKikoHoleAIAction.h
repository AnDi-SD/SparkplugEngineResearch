#pragma once
#include "Code/wxAIAction.h"
#include "Analysis/Host/wxKikoHoleAIActionHost.h"

namespace sparkplug::reconstruction { class spNode; }

namespace winx::reconstruction
{
    enum class wxKikoHoleResultForAnalysis
    {
        Completed,
        UnqualifiedEntry,
        UnqualifiedPhase2,
        UnqualifiedNode,
        UnqualifiedCommand,
    };

    // Explicit host binding for a captured PC identity. It is not an address
    // conversion, native field, resolver, or graph producer. The caller keeps
    // the plain parent and own subtree live and stable for the whole call.
    struct wxKikoHoleParentBindingForAnalysis final
    {
        std::uint32_t nativeWord = 0;
        sparkplug::reconstruction::spNode* parent = nullptr;
    };

    struct wxKikoHoleStateForAnalysis final
    {
        // Typed borrowed plain Nodes. No native32 handle is cast to a host pointer.
        sparkplug::reconstruction::spNode* ownNode = nullptr;
        sparkplug::reconstruction::spNode* playerNode = nullptr;
        // Notification payload remains a raw word. Nonzero active phase2
        // requires the separate explicit equal-parent binding below.
        std::uint32_t targetWord3B0 = 0;
        std::uint32_t phase3B4 = 0;
        std::uint32_t once3B8 = 0;
        wxKikoHoleParentBindingForAnalysis equalParentBinding;
    };

    class wxKikoHoleAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x872B81BB;
        // This is an explicit post-construction input adapter, not the native
        // protected constructor, subscription setup, or factory reproduction.
        wxKikoHoleAIAction(wxKikoHoleAIActionHost&,
            const wxKikoHoleStateForAnalysis& captured) noexcept;
        ~wxKikoHoleAIAction() override = default;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        // Admitted input is a native-PC raw32 frame: code at00, payload word18,
        // low flag byte1C. This is not the pointer-sized logical frame used by
        // spNotificationForAnalysis and has no generic broadcast admission.
        // Unknown codes require only the first four readable bytes.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        // KikoHole Entry occupies PCslot24, portable PS2-offset slot2C.
        bool vfunc_2C() noexcept override;
        wxKikoHoleResultForAnalysis UpdateForAnalysis() noexcept;
        wxKikoHoleStateForAnalysis& StateForAnalysis() noexcept { return state_; }
        const wxKikoHoleStateForAnalysis& StateForAnalysis() const noexcept { return state_; }
        wxKikoHoleResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        wxKikoHoleAIActionHost& host_;
        wxKikoHoleStateForAnalysis state_;
        wxKikoHoleResultForAnalysis lastResult_ = wxKikoHoleResultForAnalysis::Completed;
    };
}
