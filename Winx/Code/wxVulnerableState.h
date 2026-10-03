#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxVulnerableState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1B815375;
        static constexpr std::uint32_t StateSelector = 28;
        wxVulnerableState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) override;
        void vfunc_3C(const void* event) override;

        struct OwnFieldsForAnalysis { std::uint8_t field3C, field3D, field3E; };
        [[nodiscard]] OwnFieldsForAnalysis GetOwnFieldsForAnalysis() const noexcept
        { return {field3C_, field3D_, field3E_}; }
        void SetOwnFieldsForAnalysis(OwnFieldsForAnalysis fields) noexcept
        { field3C_ = fields.field3C; field3D_ = fields.field3D; field3E_ = fields.field3E; }
        // Explicit fixture mutation: construction always writes selector28.
        void SetSelectorForAnalysis(std::uint32_t selector) noexcept
        { SetStateSelectorForConstruction(selector); }
    private:
        void ExitEffects();
        std::uint8_t field3C_ = 0;
        std::uint8_t field3D_ = 0;
        std::uint8_t field3E_ = 1;
    };
}
