#pragma once
// Original registered identity and own PC/PS2 hooks; portable API is analytical.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxPhysicalAttackState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x78781786;
        static constexpr std::uint32_t StateSelector = 0x13;
        wxPhysicalAttackState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override;
        void vfunc_3C(const void*) override;
    };
}
