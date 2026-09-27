#pragma once

#include "Code/wxBaseAIBehavior.h"

namespace winx::reconstruction
{
    class wxBacoAIBehavior final : public wxBaseAIBehavior
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x11EE770A;

        struct ConstructionStateForAnalysis final
        {
            std::uint8_t flag0 = 1;
            std::uint8_t flag1 = 1;
            std::uint32_t value0 = 500;
            std::uint32_t value1 = 1500;
            std::uint8_t flag2 = 0;
            std::uint8_t flag3 = 0;
        };

        wxBacoAIBehavior() noexcept = default;
        ~wxBacoAIBehavior() override = default;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        // PC/PS2 vtable slot 17: select key 0, then forward the incoming
        // parameter to the common transition. It bypasses the action gate.
        void SelectDefaultActionForAnalysis(std::uint32_t parameter) noexcept;

        [[nodiscard]] const ConstructionStateForAnalysis&
            GetConstructionStateForAnalysis() const noexcept { return construction_; }

    private:
        ConstructionStateForAnalysis construction_;
    };
}
