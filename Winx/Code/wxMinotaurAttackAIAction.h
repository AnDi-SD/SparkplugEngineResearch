#pragma once
#include "Code/wxAIAction.h"
#include "Analysis/Host/wxMinotaurAttackAIActionHost.h"
#include <optional>

namespace winx::reconstruction
{
    enum class wxMinotaurAttackProfileForAnalysis
    { HooksOnly,PcCooldown,PcCooldownAndFiniteMovement,PcMovementWithCapturedControllerReset };
    enum class wxMinotaurAttackResultForAnalysis
    { Completed,SelectorRequested,UnqualifiedUpdateProfile,UnqualifiedPhase,UnqualifiedTargetData,UnqualifiedNumericDomain,UnqualifiedControllerReset };
    struct wxMinotaurAttackStateForAnalysis final
    {
        void* target=nullptr; //borrowed PC3A8 actor
        std::uint32_t phase=0,deadline=0,randomWord3B4=0,word3B8=0x3E99999A;
        std::uint8_t byte3BC=0,byte3BD=0;
        void* command=nullptr; //borrowed PC3C0, separately cached by Entry
        std::optional<std::uint32_t> word3C4; //constructor leaves this cell unwritten
    };
    class wxMinotaurAttackAIAction final:public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x5B6B6464;
        explicit wxMinotaurAttackAIAction(wxMinotaurAttackAIActionHost&,
            wxMinotaurAttackProfileForAnalysis=wxMinotaurAttackProfileForAnalysis::HooksOnly) noexcept;
        ~wxMinotaurAttackAIAction() override=default;
        static void SetFactoryHostForAnalysis(wxMinotaurAttackAIActionHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        void vfunc_30() noexcept override { (void)EnterForAnalysis(); }
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override { (void)QueryForAnalysis(); }
        wxMinotaurAttackResultForAnalysis EnterForAnalysis() noexcept;
        wxMinotaurAttackResultForAnalysis QueryForAnalysis() noexcept;
        wxMinotaurAttackResultForAnalysis UpdateForAnalysis() noexcept;
        wxMinotaurAttackStateForAnalysis& StateForAnalysis() noexcept { return state_; }
        const wxMinotaurAttackStateForAnalysis& StateForAnalysis() const noexcept { return state_; }
        wxMinotaurAttackResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        wxMinotaurAttackResultForAnalysis MovePhase2ForAnalysis() noexcept;
        wxMinotaurAttackAIActionHost& host_;
        wxMinotaurAttackProfileForAnalysis profile_;
        wxMinotaurAttackStateForAnalysis state_;
        wxMinotaurAttackResultForAnalysis lastResult_=wxMinotaurAttackResultForAnalysis::Completed;
    };
}
