#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxBloomDyingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x15526B2C;
        static constexpr std::uint32_t StateSelector = 11;
        wxBloomDyingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override { return sentDyingOver_ != 0; }
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override { return true; }
        [[nodiscard]] std::uint8_t GetSentDyingOverByteForAnalysis() const noexcept { return sentDyingOver_; }
        void SetSentDyingOverByteForAnalysis(std::uint8_t value) noexcept { sentDyingOver_ = value; }
    private:
        std::uint8_t sentDyingOver_ = 0;
    };
}
