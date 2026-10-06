#pragma once
// Inferred source path, own PC/PS2 methods with explicit foreign services.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxReadingState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6c548a6d;
        static constexpr std::uint32_t StateSelector = 27;
        wxReadingState() noexcept;
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
    };
}
