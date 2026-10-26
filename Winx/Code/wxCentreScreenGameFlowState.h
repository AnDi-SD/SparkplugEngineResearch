#pragma once
#include "wxGameFlowState.h"
#include "Analysis/Host/wxCentreScreenGameFlowStateHost.h"
#include <optional>

namespace winx::reconstruction
{
    // Inferred portable spelling. The physical native prefix is GameFlowState
    // at offset0, extent48. Parent construction/lifetime is qualified for PC;
    // the two own hooks have independent PC and PS2 integer/ownership evidence.
    class wxCentreScreenGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x63CF578D;
        struct CentreStateForAnalysis final
        {
            sparkplug::reconstruction::spMaterial* material3C=nullptr;
            bool material3CKnown=false; // nullptr storage is NOT a native default
            std::optional<std::uint32_t> word40,word44; // unwritten until Entry
        };
        explicit wxCentreScreenGameFlowState(wxCentreScreenGameFlowStateHost&);
        // Native destructor does not release own material3C. The host arena
        // intentionally retains it; the parent still destroys its own timer.
        ~wxCentreScreenGameFlowState() override = default;
        static void SetFactoryHostForAnalysis(wxCentreScreenGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Actual Copy is the inherited no-op, so it does not initialize or
        // transfer material3C/word40/word44 into an existing destination.
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        // Native Slot30 has a separate, unrecovered GUI/FPU body. The portable
        // boundary refuses it instead of inheriting the base's literal true.
        bool FlowSlot30ForAnalysis() override;
        [[nodiscard]] const CentreStateForAnalysis& GetCentreStateForAnalysis() const noexcept{return centre_;}
        CentreStateForAnalysis& CentreStateForFixture() noexcept{return centre_;}
    private:
        sparkplug::reconstruction::spMaterial* ReadMaterialForAnalysis() const;
        wxCentreScreenGameFlowStateHost& centreHost_;
        CentreStateForAnalysis centre_;
    };
}
