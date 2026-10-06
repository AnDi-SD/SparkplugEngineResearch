#pragma once
// Inferred path and analytical interface; own behavior recovered from PC/PS2.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxMosquitoAttackState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x6cf96918;
        static constexpr std::uint32_t StateSelector=3;
        wxMosquitoAttackState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override;
        void vfunc_3C(const void* event) override;
    };
}
