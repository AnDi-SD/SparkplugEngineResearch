#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxHangingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3AE37B41;
        static constexpr std::uint32_t StateSelector = 17;
        wxHangingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] std::uint32_t GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(std::uint32_t value) noexcept { field3C_ = value; }
    private:
        std::uint32_t field3C_ = 0;
    };
}
