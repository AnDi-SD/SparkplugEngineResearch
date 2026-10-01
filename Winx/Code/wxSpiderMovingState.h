#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxSpiderMovingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x463733DF;
        static constexpr std::uint32_t StateSelector = 0;
        wxSpiderMovingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
    };
}
