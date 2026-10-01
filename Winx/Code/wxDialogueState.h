#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxDialogueState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x16651EBE;
        static constexpr std::uint32_t StateSelector = 32;
        wxDialogueState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t target) override;
        [[nodiscard]] static std::uint32_t ComposeDialogueKeyForAnalysis(std::uint32_t input) noexcept;
    };
}
