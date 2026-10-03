#pragma once
// Inferred source declarations. Physical base and owned hooks confirmed PC/PS2.
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxDroidHurtState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x497B5830;
        static constexpr std::uint32_t StateSelector = 10;
        wxDroidHurtState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        bool vfunc_20(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        bool vfunc_34(std::uint32_t) override;
        struct RuntimeForAnalysis final { std::uint32_t deadline3C, word40; };
        [[nodiscard]] RuntimeForAnalysis GetRuntimeForAnalysis() const noexcept
        { return {deadline3C_, word40_}; }
        void SetRuntimeForAnalysis(RuntimeForAnalysis value) noexcept
        { deadline3C_ = value.deadline3C; word40_ = value.word40; }
    private:
        std::uint32_t deadline3C_ = 0;
        std::uint32_t word40_ = 0;
    };
}
