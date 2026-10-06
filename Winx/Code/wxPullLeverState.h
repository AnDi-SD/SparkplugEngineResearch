#pragma once
// Registered identity and confirmed own hooks; portable spelling is analytical.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxPullLeverState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x5B00514B;
        static constexpr std::uint32_t StateSelector = 31;
        wxPullLeverState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis&) override;
        void vfunc_2C(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
    };
}
