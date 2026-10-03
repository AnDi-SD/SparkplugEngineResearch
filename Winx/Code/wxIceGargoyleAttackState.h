#pragma once
#include "wxCharacterState.h"
namespace winx::reconstruction
{
    class wxIceGargoyleAttackState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1DE24CB8;
        static constexpr std::uint32_t StateSelector = 3;
        wxIceGargoyleAttackState() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) override;
        void vfunc_3C(const void* event) override;
        [[nodiscard]] bool GetField3CForAnalysis() const noexcept { return field3C_; }
        void SetField3CForAnalysis(bool value) noexcept { field3C_ = value; }
    private:
        // Native byte3C, initialized by both original constructors. Native
        // bytes3D..3F are padding, not additional portable runtime fields.
        bool field3C_ = false;
    };
}
