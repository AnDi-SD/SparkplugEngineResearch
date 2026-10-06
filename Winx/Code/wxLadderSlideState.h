#pragma once
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxLadderSlideState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1e9b5f25;
        static constexpr std::uint32_t StateSelector = 15;
        wxLadderSlideState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        bool vfunc_34(std::uint32_t code) override;
        std::uint32_t vfunc_38(std::uint32_t code) const override;
    };
}
