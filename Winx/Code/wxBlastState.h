#pragma once
// Inferred path. Own PC/PS2 method bodies with explicit foreign graph services.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxBlastState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x918946d1;
        static constexpr std::uint32_t StateSelector = 22;
        wxBlastState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        void vfunc_3C(const void* event) override;
    };
}
