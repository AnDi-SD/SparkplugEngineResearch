#pragma once

#include "wxCharacterState.h"
#include "../Analysis/Host/wxAttackingStateHost.h"

namespace winx::reconstruction
{
    class wxAttackingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x11B50C8E;
        static constexpr std::uint32_t StateSelector = 5;

        wxAttackingState() noexcept;
        ~wxAttackingState() override;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) const override;
        [[nodiscard]] bool vfunc_38(std::uint32_t code) const noexcept override;
        void vfunc_3C(const void* event) override;

        [[nodiscard]] bool GetEventFlagForAnalysis() const noexcept;

    private:
        [[nodiscard]] wxAttackingStateHost& RequireAttackHostForAnalysis() const;
        [[nodiscard]] bool CanSkipTransitionForAnalysis(std::uint32_t key);
        [[nodiscard]] std::uint32_t ApplyGlobalAttackModeForAnalysis(
            std::uint32_t key);
        void StartTransitionForAnalysis(wxAnimationRequestForAnalysis& request,
            std::uint32_t clearMask, std::uint32_t setBits);

        // Both original factories leave +0x3c at zero. The portable object
        // does not claim their physical ABI.
        bool eventFlag_ = false;
    };
}
