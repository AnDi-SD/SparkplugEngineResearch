#pragma once
// Inferred declarations/path; PC/PS2 own hooks and physical base are confirmed.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxBirdFlyingState final:public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x18E64F87;
        static constexpr std::uint32_t StateSelector=20;
        wxBirdFlyingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        bool vfunc_20(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        void vfunc_3C(const void*) override;
    private:
        void Play(wxAnimationRequestForAnalysis&,std::uint32_t mask,std::uint32_t bits,bool mode);
    };
}
