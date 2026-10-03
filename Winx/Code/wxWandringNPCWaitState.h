#pragma once
#include "wxNPCStateOperationsForAnalysis.h"
namespace winx::reconstruction
{
    class wxWandringNPCWaitState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x4D9470B5;
        static constexpr std::uint32_t StateSelector = 0;
        explicit wxWandringNPCWaitState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;

        [[nodiscard]] const wxNPCStateFieldsForAnalysis& GetFieldsForAnalysis() const noexcept { return fields_; }
        void SetFieldsForAnalysis(std::uint8_t flag, void* cached) noexcept { fields_.field3C = flag; fields_.field40 = cached; }
    private:
        friend struct wxNPCStateOperationsForAnalysis;
        wxNPCStateFieldsForAnalysis fields_;
    };
}
