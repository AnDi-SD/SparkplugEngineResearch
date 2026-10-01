#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxGlyphState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1928C37D;
        static constexpr std::uint32_t StateSelector = 37;
        wxGlyphState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t target) override;
        [[nodiscard]] static std::uint32_t ComposeKeyForAnalysis(std::uint32_t key,std::uint32_t classification) noexcept;
    };
}
