#pragma once
// Inferred source path, native PC/PS2 own-method component.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxHurtState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x37c025bf;
        static constexpr std::uint32_t StateSelector = 10;
        wxHurtState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const override;
        [[nodiscard]] std::uint8_t GetByte3CForAnalysis() const noexcept { return byte3C_; }
        void SetByte3CForAnalysis(std::uint8_t value) noexcept { byte3C_ = value; }
    private:
        std::uint8_t byte3C_ = 0; // Own entry reads this; source-level meaning open.
    };
}
