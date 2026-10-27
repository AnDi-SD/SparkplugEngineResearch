#pragma once
#include "Code/wxAIAction.h"
#include "Code/wxIceGargoyleAIActionStateForAnalysis.h"

namespace winx::reconstruction
{
    class wxIceGargoyleWithdrawlAIAction final:public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x239516B1;
        explicit wxIceGargoyleWithdrawlAIAction(wxIceGargoyleAIActionHost&,
            wxIceGargoyleUpdateProfileForAnalysis=wxIceGargoyleUpdateProfileForAnalysis::HooksOnly,
            wxPerceptionNumericProfileForAnalysis=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
        ~wxIceGargoyleWithdrawlAIAction() override=default;
        static void SetFactoryHostForAnalysis(wxIceGargoyleAIActionHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override {} //literal no-op, no forwarding
        bool vfunc_24() noexcept override;
        // Remaining bool slot uses the inherited actual literal true body.
        void vfunc_30() noexcept override { (void)EnterForAnalysis(); }
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override { (void)QueryForAnalysis(); }
        wxIceGargoyleResultForAnalysis EnterForAnalysis() noexcept;
        wxIceGargoyleResultForAnalysis QueryForAnalysis() noexcept;
        wxIceGargoyleResultForAnalysis UpdateForAnalysis() noexcept;
        wxIceGargoyleAIActionStateForAnalysis& StateForAnalysis() noexcept { return state_; }
        const wxIceGargoyleAIActionStateForAnalysis& StateForAnalysis() const noexcept { return state_; }
        wxIceGargoyleResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        wxIceGargoyleAIActionHost& host_;
        wxIceGargoyleUpdateProfileForAnalysis updateProfile_;
        wxPerceptionNumericProfileForAnalysis perceptionProfile_;
        wxIceGargoyleAIActionStateForAnalysis state_;
        wxIceGargoyleResultForAnalysis lastResult_=wxIceGargoyleResultForAnalysis::Completed;
    };
}

