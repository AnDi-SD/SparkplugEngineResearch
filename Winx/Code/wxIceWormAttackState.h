#pragma once
// Inferred file path; native PC/PS2 state has no fields beyond wxCharacterState.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxIceWormAttackState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x709e322b;
        static constexpr std::uint32_t StateSelector = 3;
        wxIceWormAttackState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        void vfunc_3C(const void* event) override;
    };
}
