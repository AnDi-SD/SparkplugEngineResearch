#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxMinotaurMovingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x5E63555F;
        static constexpr std::uint32_t StateSelector = 0;
        wxMinotaurMovingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) override;
        [[nodiscard]] std::uint8_t GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(std::uint8_t value) noexcept { field3C_ = value; }
    private:
        // Native size40, byte3C initialized to zero on both platforms. The
        // portable object does not claim native layout or padding contents.
        std::uint8_t field3C_ = 0;
    };
}
