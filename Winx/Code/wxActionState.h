#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxActionState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x196333F8;
        static constexpr std::uint32_t StateSelector = 25;

        wxActionState() noexcept;
        ~wxActionState() override;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(
            wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(
            wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;

    private:
        [[nodiscard]] bool StartMaskedTransitionForAnalysis(
            wxAnimationRequestForAnalysis& request, std::uint32_t mask,
            std::uint32_t setBits, bool clearFlag1C);
    };
}
