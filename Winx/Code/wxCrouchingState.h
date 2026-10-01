#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxCrouchingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x056B1039;
        static constexpr std::uint32_t StateSelector = 2;
        wxCrouchingState() noexcept;
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
        [[nodiscard]] static std::uint32_t ComposeCrouchingKeyForAnalysis(
            std::uint32_t input, float motion) noexcept;
    private:
        void StartTransitionForAnalysis(wxAnimationRequestForAnalysis& request,
            std::uint32_t mask, std::uint32_t bits);
    };
}
