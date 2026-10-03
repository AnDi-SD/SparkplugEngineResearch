#pragma once
// Inferred source declarations; PC/PS2 physical base and owned hooks confirmed.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxSpiderHurtState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x54F716CC;
        static constexpr std::uint32_t StateSelector = 10;
        wxSpiderHurtState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        bool vfunc_20(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        bool vfunc_34(std::uint32_t) override;
        struct RuntimeForAnalysis final
        {
            bool flag3C = false, flag3D = false;
            std::uint32_t word40 = 0, delay44 = 10000, deadline48 = 0;
            bool permission4C = true;
        };
        [[nodiscard]] RuntimeForAnalysis GetRuntimeForAnalysis() const noexcept { return runtime_; }
        void SetRuntimeForAnalysis(const RuntimeForAnalysis& value) noexcept { runtime_ = value; }
    private:
        RuntimeForAnalysis runtime_;
    };
}
