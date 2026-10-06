#pragma once
// Inferred file path and analytical declarations, paired PC/PS2 behavior.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxFrogAttackState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x4ee03d15;
        static constexpr std::uint32_t StateSelector=3;
        wxFrogAttackState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void* notification) noexcept override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override;
        void vfunc_3C(const void* event) override;
        [[nodiscard]] std::uint8_t GetByte3CForAnalysis() const noexcept{return byte3C_;}
        void SetByte3CForAnalysis(std::uint8_t value) noexcept{byte3C_=value;}
    private:
        std::uint8_t byte3C_=0; // Written on275C notification; further use unknown.
    };
}
