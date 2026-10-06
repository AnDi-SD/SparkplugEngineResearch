#pragma once
// Original registered identity; portable source spelling is analytical.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxGhoulJumpingState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x37086498;
        static constexpr std::uint32_t StateSelector = 1;
        wxGhoulJumpingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_2C(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        // Values are native literals, with no invented enum interpretation.
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override;
        void vfunc_3C(const void*) override;
    };
}
