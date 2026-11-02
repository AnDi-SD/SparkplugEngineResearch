#include "wxYetiAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include "wxGameTimer.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateYetiAttack()
        { throw std::logic_error("YetiAttack requires explicit captured state and command bindings"); }
        const spRTTIRecord record{wxYetiAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxYetiAttackAIAction", &wxAIAction::StaticRTTI(), &CreateYetiAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxYetiAttackAIAction::wxYetiAttackAIAction(wxAIActionCommandExitHost& host, void* owner,
        const CapturedState& state, ProfileForAnalysis profile) noexcept : host_(host), state_(state), profile_(profile)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxYetiAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxYetiAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxYetiAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("YetiAttack Clone remains unqualified"); }
    bool wxYetiAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxYetiAttackAIAction::ResultForAnalysis wxYetiAttackAIAction::ExitForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        auto* const owner = GetOwnerForAnalysis();
        if (!owner) return lastResult_ = ResultForAnalysis::MissingOwner;
        const auto actor = host_.ReadActor144ForAnalysis(owner);
        if (!actor || !*actor) return lastResult_ = ResultForAnalysis::MissingActorStorage;
        const auto command = host_.ReadCommand130ForAnalysis(*actor);
        if (!command || !*command) return lastResult_ = ResultForAnalysis::MissingCommandStorage;
        // Table-selected shared body5B79D0; original declaration owner open.
        if ((*command)->ResetForAnalysis() != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        state_.borrowedTarget3A8 = nullptr;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxYetiAttackAIActionPhaseResultForAnalysis wxYetiAttackAIAction::Phase54ForAnalysis() noexcept
    {
        using PhaseResult = wxYetiAttackAIActionPhaseResultForAnalysis;
        auto* const binding = phaseBinding_;
        if (!binding || binding->profile != wxYetiAttackAIActionPhaseProfileForAnalysis::PcCapturedStablePhase
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastPhaseResult_ = PhaseResult::UnqualifiedProfile;
        const auto word = [this](std::size_t offset) noexcept
        {
            std::uint32_t value = 0;
            for (unsigned byte = 0; byte != 4; ++byte)
                value |= std::uint32_t{state_.ownBytes3AC[offset - 0x3AC + byte]} << (8 * byte);
            return value;
        };
        const auto storeWord = [this](std::size_t offset, std::uint32_t value) noexcept
        {
            for (unsigned byte = 0; byte != 4; ++byte)
                state_.ownBytes3AC[offset - 0x3AC + byte] = static_cast<std::uint8_t>(value >> (8 * byte));
        };
        const auto commandByte = [this, binding](void* owner, std::uint8_t value) noexcept
        {
            if (!owner) return lastPhaseResult_ = PhaseResult::MissingOwner;
            auto* const storage = binding->ownerStorage;
            if (!storage) return lastPhaseResult_ = PhaseResult::MissingOwnerStorage;
            const auto actor = storage->ReadActor144ForAnalysis(owner);
            if (!actor || !*actor) return lastPhaseResult_ = PhaseResult::MissingActorStorage;
            const auto command = storage->ReadCommand130ForAnalysis(*actor);
            if (!command || !*command) return lastPhaseResult_ = PhaseResult::MissingCommandStorage;
            if ((*command)->StoreByte23ForAnalysis(value) != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
                return lastPhaseResult_ = PhaseResult::CommandNotAdmitted;
            return lastPhaseResult_ = PhaseResult::Completed;
        };
        const auto phase = word(0x3B0);
        if (phase == 0)
        {
            // Native5B0170 captures owner before5B0173 clears this word.
            auto* const owner = GetOwnerForAnalysis();
            storeWord(0x3BC, 0);
            return commandByte(owner, 1);
        }
        if (phase == 1)
        {
            const auto flag = state_.ownBytes3AC[0x3B8 - 0x3AC];
            state_.ownBytes3AC[0x3C0 - 0x3AC] = 1;
            if (flag != 0)
            {
                if (word(0x3E4) == 0)
                {
                    auto* const timer = binding->existingTimer;
                    if (!timer) return lastPhaseResult_ = PhaseResult::MissingExistingTimer;
                    // Stored spTaskTimer state, not a new clock sample. Native
                    // uint32 addition wraps; there is no expiry comparison.
                    const auto current = timer->GetStateForAnalysis().currentMilliseconds;
                    storeWord(0x3E4, current + std::uint32_t{800});
                }
                state_.ownBytes3AC[0x3B8 - 0x3AC] = 0;
            }
            auto* const owner = GetOwnerForAnalysis();
            return commandByte(owner, 0);
        }
        storeWord(0x3AC, 0);
        storeWord(0x3B0, 0);
        storeWord(0x3C4, 0);
        state_.ownBytes3AC[0x3C8 - 0x3AC] = 1;
        state_.ownBytes3AC[0x3B8 - 0x3AC] = 1;
        return lastPhaseResult_ = PhaseResult::Completed;
    }
    void wxYetiAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxYetiAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxYetiAttackAIAction::RefuseHookForAnalysis() noexcept { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxYetiAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxYetiAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxYetiAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxYetiAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxYetiAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxYetiAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxYetiAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
