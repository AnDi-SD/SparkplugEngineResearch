#include "wxTrollBeforeFightAIAction.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    using Result = wxTrollBeforeFightTickResultForAnalysis;
    using Profile = wxTrollBeforeFightProfileForAnalysis;
    namespace
    {
        wxTrollBeforeFightAIActionHost* factoryHost = nullptr;
        Profile factoryProfile = Profile::PC;
        std::unique_ptr<spBaseObject> CreateTroll()
        {
            if (!factoryHost) throw std::logic_error("TrollBeforeFight requires a factory host");
            return std::make_unique<wxTrollBeforeFightAIAction>(*factoryHost, factoryProfile);
        }
        const spRTTIRecord record{wxTrollBeforeFightAIAction::ClassID, wxAIAction::ClassID,
            "wxTrollBeforeFightAIAction", &wxAIAction::StaticRTTI(), &CreateTroll, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxTrollBeforeFightAIAction::wxTrollBeforeFightAIAction(wxTrollBeforeFightAIActionHost& host, Profile profile) noexcept
        : host_(host), profile_(profile) {}
    void wxTrollBeforeFightAIAction::SetFactoryHostForAnalysis(wxTrollBeforeFightAIActionHost* host, Profile profile) noexcept
    { factoryHost = host; factoryProfile = profile; }
    const spRTTIRecord& wxTrollBeforeFightAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxTrollBeforeFightAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxTrollBeforeFightAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxTrollBeforeFightAIAction>(host_, profile_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool wxTrollBeforeFightAIAction::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxTrollBeforeFightAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination, manager)) return false;
        target->phase_ = phase_; target->nextDecision_ = nextDecision_;
        target->latch_ = latch_; target->cooldown_ = cooldown_;
        return true;
    }
    void wxTrollBeforeFightAIAction::vfunc_0C(const void*) noexcept {} // no forwarding
    void wxTrollBeforeFightAIAction::vfunc_30() noexcept { phase_ = latch_ = nextDecision_ = cooldown_ = 0; }
    void wxTrollBeforeFightAIAction::vfunc_38() noexcept {}
    void wxTrollBeforeFightAIAction::WriteCommandForAnalysis(std::uint32_t offset, std::uint8_t value) noexcept
    { host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()), offset, value); }
    void wxTrollBeforeFightAIAction::vfunc_34_ClearForAnalysis() noexcept
    { for (auto offset : {0x1DU, 0x20U, 0x21U, 0x22U, 0x23U, 0x24U}) WriteCommandForAnalysis(offset, 0); }
    void wxTrollBeforeFightAIAction::QueryForAnalysis() noexcept
    {
        const auto state = host_.ReadCharacterStateWordForAnalysis(GetOwnerForAnalysis());
        if (phase_ < 1 || phase_ > 3) return;
        if (latch_ == 0) { if (state == phase_ + 4) latch_ = 1; }
        else if (latch_ == 1 && state == 0) latch_ = 2;
    }
    bool wxTrollBeforeFightAIAction::vfunc_28() noexcept { QueryForAnalysis(); return true; }
    void wxTrollBeforeFightAIAction::CommandStageForAnalysis(std::uint32_t offset, bool setCooldown) noexcept
    {
        if (latch_ == 0)
        {
            if (setCooldown) cooldown_ = host_.GameTimerPrefixForAnalysis().GetStateForAnalysis().currentMilliseconds + std::uint32_t{12000};
            WriteCommandForAnalysis(offset, 1);
        }
        else if (latch_ == 1) WriteCommandForAnalysis(offset, 0);
        else phase_ = latch_ = 0;
    }
    Result wxTrollBeforeFightAIAction::Stage0ForAnalysis() noexcept
    {
        if (nextDecision_ == 0)
        {
            host_.ResetCommandForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()));
            const auto now = host_.GameTimerPrefixForAnalysis().GetStateForAnalysis().currentMilliseconds;
            const auto random = host_.RandomStateForAnalysis().Next();
            nextDecision_ = now + std::uint32_t{1000} + random % std::uint32_t{2501};
            return Result::Completed;
        }
        const auto now = host_.GameTimerPrefixForAnalysis().GetStateForAnalysis().currentMilliseconds;
        if (now <= nextDecision_) return Result::Completed;
        const auto random = host_.RandomStateForAnalysis().Next();
        if (profile_ == Profile::PS2IntegerBoundaries) return Result::UnqualifiedPs2RandomPredicate;
        // Native PC fild/u32 correction, exact power-of-two multiply and0.5
        // comparison have no rounding loss for32-bit input in x87.
        std::uint32_t nextPhase = 1;
        if (random < 0x80000000U)
        {
            auto& timer = host_.GameTimerPrefixForAnalysis();
            const auto cooldown = cooldown_;
            const auto current = timer.GetStateForAnalysis().currentMilliseconds;
            if (cooldown < current) nextPhase = 2;
        }
        phase_ = nextPhase;
        latch_ = nextDecision_ = 0;
        return Result::Completed;
    }
    Result wxTrollBeforeFightAIAction::TickForAnalysis() noexcept
    {
        if (phase_ == 1) CommandStageForAnalysis(0x22, false);
        else if (phase_ == 2) CommandStageForAnalysis(0x23, true);
        else if (phase_ == 3) CommandStageForAnalysis(0x24, false);
        else return Stage0ForAnalysis();
        return Result::Completed;
    }
    bool wxTrollBeforeFightAIAction::vfunc_24() noexcept
    { return TickForAnalysis() == Result::Completed; } // false is our explicit PS2 guard
}
