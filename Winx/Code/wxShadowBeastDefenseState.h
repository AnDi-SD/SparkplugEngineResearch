#pragma once
#include "wxCharacterState.h"
#include <optional>
namespace winx::reconstruction
{
    class wxShadowBeastDefenseState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x43634D72;
        static constexpr std::uint32_t StateSelector = 8;
        wxShadowBeastDefenseState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] std::uint8_t GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(std::uint8_t value) noexcept { field3C_ = value; }
        void SetControllerForAnalysis(void* controller) noexcept { field40_ = controller; }
        [[nodiscard]] const std::optional<void*>& GetControllerForAnalysis() const noexcept { return field40_; }
    private:
        [[nodiscard]] void* RequireControllerForAnalysis() const;
        std::uint8_t field3C_ = 0;
        // Original constructor DOES NOT write word40. Empty optional marks
        // unknown storage in our analysis object; it is not a native null.
        std::optional<void*> field40_;
    };
}
