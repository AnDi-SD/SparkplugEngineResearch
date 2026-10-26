#pragma once
#include "Code/wxAIAction.h"
#include "Analysis/Host/wxTrollBeforeFightAIActionHost.h"
namespace winx::reconstruction
{
    enum class wxTrollBeforeFightProfileForAnalysis { PC, PS2IntegerBoundaries };
    enum class wxTrollBeforeFightTickResultForAnalysis : std::uint32_t
    { Completed = 0, UnqualifiedPs2RandomPredicate = 1 };
    class wxTrollBeforeFightAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x03197B06;
        explicit wxTrollBeforeFightAIAction(wxTrollBeforeFightAIActionHost&,
            wxTrollBeforeFightProfileForAnalysis = wxTrollBeforeFightProfileForAnalysis::PC) noexcept;
        static void SetFactoryHostForAnalysis(wxTrollBeforeFightAIActionHost*,
            wxTrollBeforeFightProfileForAnalysis = wxTrollBeforeFightProfileForAnalysis::PC) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        void vfunc_0C(const void*) noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        wxTrollBeforeFightTickResultForAnalysis TickForAnalysis() noexcept;
        void QueryForAnalysis() noexcept;
        std::uint32_t PhaseForAnalysis() const noexcept { return phase_; }
        std::uint32_t LatchForAnalysis() const noexcept { return latch_; }
        std::uint32_t NextDecisionForAnalysis() const noexcept { return nextDecision_; }
        std::uint32_t CooldownForAnalysis() const noexcept { return cooldown_; }
        void SetStateForAnalysis(std::uint32_t phase, std::uint32_t latch,
            std::uint32_t nextDecision, std::uint32_t cooldown) noexcept
        { phase_ = phase; latch_ = latch; nextDecision_ = nextDecision; cooldown_ = cooldown; }
    private:
        wxTrollBeforeFightTickResultForAnalysis Stage0ForAnalysis() noexcept;
        void CommandStageForAnalysis(std::uint32_t offset, bool setCooldown) noexcept;
        void WriteCommandForAnalysis(std::uint32_t offset, std::uint8_t value) noexcept;
        wxTrollBeforeFightAIActionHost& host_;
        wxTrollBeforeFightProfileForAnalysis profile_;
        std::uint32_t phase_ = 0, latch_ = 0, nextDecision_ = 0, cooldown_ = 0;
    };
}
