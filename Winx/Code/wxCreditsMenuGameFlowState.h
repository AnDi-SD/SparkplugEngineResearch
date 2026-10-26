#pragma once

#include "wxGameFlowState.h"
#include "Analysis/Host/wxCreditsMenuGameFlowStateHost.h"
#include "Code/SparkBase/spNotification.h"

namespace winx::reconstruction
{
    // Inferred portable declaration. Native extent44, physical parent3C.
    // Construction/lifetime reuse the PC parent. Own integer hooks are also
    // known for PS2; no claim that this constructor models PS2 global defaults.
    class wxCreditsMenuGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0xC564A54A;
        struct CreditsStateForAnalysis final
        {
            void* cachedInput3C=nullptr; // borrowed, separately loaded from singleton+18
            std::uint8_t field40=0,field41=1;
            // Native42/43 remain unwritten. They are not modeled as zero data.
        };
        explicit wxCreditsMenuGameFlowState(wxCreditsMenuGameFlowStateHost&);
        ~wxCreditsMenuGameFlowState() override;
        static void SetFactoryHostForAnalysis(wxCreditsMenuGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Inherited Copy is the actual base no-op: existing destination flags,
        // input, timer and host binding are not copied from the source.
        void vfunc_0C(const void* portableNotification) noexcept override;
        void NotifyCreditsForAnalysis(const sparkplug::reconstruction::spNotificationForAnalysis&);

        void FlowSlot20ForAnalysis() override;
        void FlowSlot2CForAnalysis() override;
        bool FlowSlot30ForAnalysis() override;
        void CompleteCreditsForAnalysis(); // original PC5EC5A0; PS2 inlined sequence
        CreditsStateForAnalysis& CreditsStateForFixture() noexcept { return credits_; }
        [[nodiscard]] const CreditsStateForAnalysis& GetCreditsStateForAnalysis() const noexcept { return credits_; }
    private:
        wxCreditsMenuGameFlowStateHost& creditsHost_;
        CreditsStateForAnalysis credits_;
    };
}
