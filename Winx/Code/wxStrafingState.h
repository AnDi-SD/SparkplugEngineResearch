#pragma once
#include "wxCharacterState.h"

namespace winx::reconstruction
{
    class wxStrafingState : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x22474D5C;
        static constexpr std::uint32_t StateSelector = 0;
        wxStrafingState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        using wxCharacterState::vfunc_20;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_20(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] void* GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(void* value) noexcept { field3C_ = value; }
    private:
        void* field3C_ = nullptr;
    };
}
