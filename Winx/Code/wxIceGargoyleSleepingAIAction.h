#pragma once
#include "Code/wxAIAction.h"
#include "Analysis/Host/wxIceGargoyleSleepingAIActionHost.h"

namespace winx::reconstruction
{
    enum class wxSleepingTargetProfileForAnalysis { ExistingOnly, BoundedPcAcquisition };
    enum class wxSleepingTickResultForAnalysis : std::uint32_t
    {
        Completed = 0,
        SelectorBoundary = 1,
        UnqualifiedTargetManagerAcquisition = 2
    };

    class wxIceGargoyleSleepingAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3C7B1A31;
        explicit wxIceGargoyleSleepingAIAction(wxIceGargoyleSleepingAIActionHost&,
            wxSleepingTargetProfileForAnalysis = wxSleepingTargetProfileForAnalysis::ExistingOnly) noexcept;
        static void SetFactoryHostForAnalysis(wxIceGargoyleSleepingAIActionHost*,
            wxSleepingTargetProfileForAnalysis = wxSleepingTargetProfileForAnalysis::ExistingOnly) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;

        void WakeForAnalysis() noexcept;
        wxSleepingTickResultForAnalysis TickForAnalysis() noexcept;
        std::uint32_t PhaseForAnalysis() const noexcept { return phase_; }
        std::uint32_t StampForAnalysis() const noexcept { return stamp_; }
        void SetStateForAnalysis(std::uint32_t phase, std::uint32_t stamp) noexcept
        { phase_ = phase; stamp_ = stamp; }

    private:
        void Stage0ForAnalysis() noexcept;
        wxSleepingTickResultForAnalysis Stage1ForAnalysis() noexcept;
        wxIceGargoyleSleepingAIActionHost& host_;
        wxSleepingTargetProfileForAnalysis targetProfile_;
        std::uint32_t phase_ = 0;
        std::uint32_t stamp_ = 0;
    };
}
