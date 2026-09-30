#pragma once

#include "wxCharacterState.h"
#include "../Analysis/Host/wxBasicMovingStateHost.h"

namespace winx::reconstruction
{
    // PC/PS2 state selector zero. The non-nine movement request path is
    // recovered; selector nine still requires original random/service calls.
    class wxBasicMovingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1D533B89;

        wxBasicMovingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        void SetCurrentSelectorForAnalysis(std::uint32_t selector) noexcept
        { SetStateSelectorForConstruction(selector); }
        void SetMovementBindingsForAnalysis(void* owner, void* consumer,
            wxBasicMovingStateHost* host) noexcept
        {
            movementHost_ = host;
            SetBindingsForAnalysis(owner, consumer, host);
        }
        [[nodiscard]] static std::uint32_t ComposeMovingKeyForAnalysis(
            std::uint32_t input, float magnitude, bool flag) noexcept;

        // Native slots 12 and 13. Slot 13 is the measured permission branch.
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t target) const override;

    private:
        wxBasicMovingStateHost* movementHost_ = nullptr;
    };
}
