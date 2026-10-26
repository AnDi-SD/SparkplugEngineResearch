#pragma once

#include "Code/wxAIAction.h"
#include "Code/wxPerception.h"
#include "Analysis/Host/wxIdleAIActionHost.h"

namespace winx::reconstruction
{
    // Portable behavior component. The host and numeric profile are our
    // integration configuration, not newly inferred native payload fields.
    class wxIdleAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3EA07007;
        explicit wxIdleAIAction(wxIdleAIActionHost&, wxPerceptionNumericProfileForAnalysis
            = wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64) noexcept;
        static void SetFactoryHostForAnalysis(wxIdleAIActionHost*, wxPerceptionNumericProfileForAnalysis
            = wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;

        // Unlike the base action, Idle neither forwards messages nor clears
        // its inherited clearable word on exit.
        void vfunc_0C(const void*) noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;

    private:
        wxIdleAIActionHost& host_;
        wxPerceptionNumericProfileForAnalysis numericProfile_;
    };
}
