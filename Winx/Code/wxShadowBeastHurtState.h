#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxShadowBeastHurtState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x0C87213F;
        static constexpr std::uint32_t StateSelector = 10;
        wxShadowBeastHurtState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t target) override;
    };
}
