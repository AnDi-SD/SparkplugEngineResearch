#pragma once

#include "Code/wxAIAction.h"
#include "Analysis/Host/wxAttackAIActionHost.h"

#include <cstdint>

namespace winx::reconstruction
{
    // Portable reconstruction of the verified Attack action contract.
    // Native object storage and the second inline PathFinder are documented in
    // Analysis/{PC,PS2}; this object is not a native memory overlay.
    class wxAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6515353D;

        explicit wxAttackAIAction(wxAttackAIActionHost& host) noexcept;
        ~wxAttackAIAction() override = default;

        static void SetFactoryHostForAnalysis(wxAttackAIActionHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        void vfunc_0C(const void* notification) noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        [[nodiscard]] bool vfunc_24() noexcept override; // native v7: state dispatch
        void vfunc_30() noexcept override;                 // native v10: enter
        void vfunc_34_ClearForAnalysis() noexcept override; // native v11: exit
        void vfunc_38() noexcept override;                 // native v12: target query

        // Observational controls for fields whose original setters belong to
        // still-unrecovered state handlers. These names are not game symbols.
        [[nodiscard]] void* GetTargetForAnalysis() const noexcept { return target_; }
        [[nodiscard]] std::uint32_t GetStateForAnalysis() const noexcept { return state_; }
        [[nodiscard]] std::uint32_t GetDeadlineForAnalysis() const noexcept { return deadline_; }
        [[nodiscard]] std::uint32_t GetField3B4ForAnalysis() const noexcept { return field3B4_; }
        [[nodiscard]] std::uint32_t GetField3B8ForAnalysis() const noexcept { return field3B8_; }
        [[nodiscard]] std::uint8_t GetFlag3F0ForAnalysis() const noexcept { return flag3F0_; }
        void SetOwnFieldsForAnalysis(void* target, std::uint32_t state,
            std::uint32_t deadline, std::uint32_t field3B4,
            std::uint32_t field3B8, std::uint8_t flag3F0) noexcept;

    private:
        wxAttackAIActionHost& host_;
        void* target_ = nullptr; // borrowed
        std::uint32_t state_ = 1;
        std::uint32_t deadline_ = 0;
        std::uint32_t field3B4_ = 0;
        std::uint32_t field3B8_ = 0;
        std::uint8_t flag3F0_ = 0;
    };
}
