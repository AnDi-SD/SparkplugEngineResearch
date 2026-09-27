#pragma once

#include "Analysis/Host/wxBacoAttackAIActionHost.h"
#include "Code/wxAIAction.h"

#include <cstdint>

namespace winx::reconstruction
{
    // Portable reconstruction of measured Baco action fields and entry/exit.
    // Active state bodies and native storage remain separate analysis concerns.
    class wxBacoAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x013B1195;

        explicit wxBacoAttackAIAction(wxBacoAttackAIActionHost& host) noexcept;
        ~wxBacoAttackAIAction() override = default;

        static void SetFactoryHostForAnalysis(wxBacoAttackAIActionHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        void vfunc_0C(const void* notification) noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        [[nodiscard]] bool vfunc_24() noexcept override;   // native v7
        void vfunc_30() noexcept override;                   // native v10, enter
        void vfunc_34_ClearForAnalysis() noexcept override; // native v11, exit
        void vfunc_38() noexcept override;                   // native v12, target query

        [[nodiscard]] void* GetTargetForAnalysis() const noexcept { return target_; }
        [[nodiscard]] std::uint32_t GetStateForAnalysis() const noexcept { return state_; }
        [[nodiscard]] std::uint32_t GetField3B0ForAnalysis() const noexcept { return field3B0_; }
        [[nodiscard]] std::uint32_t GetField3B4ForAnalysis() const noexcept { return field3B4_; }
        [[nodiscard]] std::uint32_t GetField3B8ForAnalysis() const noexcept { return field3B8_; }
        [[nodiscard]] std::uint8_t GetFlag3BCForAnalysis() const noexcept { return flag3BC_; }
        void SetOwnFieldsForAnalysis(void* target, std::uint32_t state,
            std::uint32_t field3B0, std::uint32_t field3B4,
            std::uint32_t field3B8, std::uint8_t flag3BC) noexcept;

    private:
        wxBacoAttackAIActionHost& host_;
        void* target_ = nullptr; // borrowed
        std::uint32_t state_ = 4;
        std::uint32_t field3B0_ = 0;
        std::uint32_t field3B4_ = 0;
        std::uint32_t field3B8_ = 0x3CF5C28F; // 0.03f bits
        std::uint8_t flag3BC_ = 0;
    };
}
