#pragma once
#include "wxGameFlowState.h"
#include "wxLogoSharedResourceLoading.h"
#include "Analysis/Host/wxLogoAnimGameFlowStateHost.h"
#include "Code/Sparkplug/spTimer.h"
#include <optional>

namespace winx::reconstruction
{
    // Inferred portable declaration, not native byte ABI. Parent construction
    // is PC; own hook profile differences are explicitly bounded below.
    class wxLogoAnimGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x2B43A28A;
        struct LogoStateForAnalysis final
        {
            void* cachedInput3C=nullptr; // second singleton member18 borrow
            std::uint8_t field40=1,field41=0;
            std::uint32_t word44=0,word48=0,word4C=0;
            sparkplug::reconstruction::spBaseObject* referenced50=nullptr;
            std::uint32_t word54=0,word7C=0,word80=0;
            std::optional<std::uint32_t> word84; // native constructor does not write it
            // Native42/43 and timer padding remain unwritten, not zero defaults.
        };
        explicit wxLogoAnimGameFlowState(wxLogoAnimGameFlowStateHost&);
        ~wxLogoAnimGameFlowState() override;
        static void SetFactoryHostForAnalysis(wxLogoAnimGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Inherited Copy is the literal root no-op, including all own state,
        // borrowed input, inline timer and referenced50.
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        void FlowSlot2CForAnalysis() override;
        bool FlowSlot30ForAnalysis() override; // unrecovered resource/FPU body refuses
        const LogoStateForAnalysis& GetLogoStateForAnalysis() const noexcept{return logo_;}
        LogoStateForAnalysis& LogoStateForFixture() noexcept{return logo_;}
        [[nodiscard]] bool HasLiveLogoTimerForAnalysis() const noexcept{return timer_.has_value();}
        sparkplug::reconstruction::spTimer& LogoTimerForAnalysis();
        const sparkplug::reconstruction::spTimer& LogoTimerForAnalysis() const;
    private:
        void ReleaseLogoReferenceForAnalysis(bool clearAfter);
        wxLogoAnimGameFlowStateHost& logoHost_;
        LogoStateForAnalysis logo_;
        // Explicit lifetime lets dtor preserve native order: unsubscribe,
        // inline timer destruction, reference50 release, parent destruction.
        std::optional<sparkplug::reconstruction::spTimer> timer_{std::in_place};
    };
}
