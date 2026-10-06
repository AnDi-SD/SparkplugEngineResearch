#pragma once
// Inferred path and analytical interface; paired original PC/PS2 behavior.
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxOpenSecretPassageState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x0a9b5545;
        static constexpr std::uint32_t StateSelector=41;
        wxOpenSecretPassageState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis&) override;
        void vfunc_30(wxAnimationRequestForAnalysis&) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t) override;
        [[nodiscard]] std::uint32_t vfunc_38(std::uint32_t) const noexcept override;
        void vfunc_3C(const void* event) override;
    };
}
