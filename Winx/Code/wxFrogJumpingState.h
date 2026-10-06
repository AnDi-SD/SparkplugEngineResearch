#pragma once
#include "wxCharacterState.h"
#include <optional>

namespace winx::reconstruction
{
    class wxFrogJumpingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x47507873;
        static constexpr std::uint32_t StateSelector = 1;
        wxFrogJumpingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) override;
        void vfunc_3C(const void* event) override;

        // Original constructor leaves byte3C unwritten. nullopt records that
        // fact; native entry or an explicit fixture supplies a value.
        [[nodiscard]] std::optional<std::uint8_t> GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(std::uint8_t value) noexcept { field3C_ = value; }
        [[nodiscard]] float GetField40ForAnalysis() const noexcept { return field40_; }
        void SetField40ForAnalysis(float value) noexcept { field40_ = value; }
    private:
        std::optional<std::uint8_t> field3C_;
        float field40_ = 375.0f;
    };
}
