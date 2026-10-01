#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    // PC/PS2 registered type and native hooks; portable layout, not an ABI overlay.
    class wxShadowBeastJumpingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x303A77C2;
        static constexpr std::uint32_t StateSelector = 1;
        wxShadowBeastJumpingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t target) override;
    };
}
