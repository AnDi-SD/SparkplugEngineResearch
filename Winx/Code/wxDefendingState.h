#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxDefendingState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x38E64587;
        static constexpr std::uint32_t StateSelector = 8;
        wxDefendingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        // Analytical names of nonvirtual original helpers, not source names.
        void InitializeNodesForAnalysis();
        void SetNodesEnabledForAnalysis(bool enabled, bool silent);
        void AdvanceExitNodesForAnalysis();
        struct FieldsForAnalysis final
        {
            void* field3C; void* field40; void* field44;
            float field48; std::uint32_t field4C; std::uint8_t field50;
        };
        [[nodiscard]] FieldsForAnalysis GetFieldsForAnalysis() const noexcept
        { return {field3C_, field40_, field44_, field48_, field4C_, field50_}; }
        void SetFieldsForAnalysis(const FieldsForAnalysis& values) noexcept
        { field3C_=values.field3C; field40_=values.field40; field44_=values.field44;
          field48_=values.field48; field4C_=values.field4C; field50_=values.field50; }
    private:
        void* field3C_ = nullptr;
        void* field40_ = nullptr;
        void* field44_ = nullptr;
        float field48_ = 0;
        std::uint32_t field4C_ = 0;
        std::uint8_t field50_ = 0;
    };
}
