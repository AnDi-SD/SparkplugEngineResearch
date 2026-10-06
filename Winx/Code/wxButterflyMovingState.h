#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxButterflyMovingState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x46B6517E;
        static constexpr unsigned StateSelector = 0;
        wxButterflyMovingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(unsigned) override { return false; }
        struct FieldsForAnalysis
        {
            std::array<float, 3> origin{}, target{};
            float desiredHeight = 0;
            std::array<float, 3> current{};
            unsigned initialized = 0, deadline = 0;
            std::uint8_t phase = 1;
        };
        [[nodiscard]] FieldsForAnalysis GetFieldsForAnalysis() const noexcept { return fields_; }
        void SetFieldsForAnalysis(const FieldsForAnalysis& value) noexcept { fields_ = value; }
        // PC51F390/51F530 and51F010; analytical helper names.
        void AdvancePhaseForAnalysis();
        void MoveForAnalysis();
    private:
        FieldsForAnalysis fields_;
        void RandomHeight();
    };
}
