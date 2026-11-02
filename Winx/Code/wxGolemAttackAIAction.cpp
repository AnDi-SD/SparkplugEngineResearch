#include "wxGolemAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include "wxCharacterRegistry.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateGolemAttack()
        { throw std::logic_error("GolemAttack requires explicit captured state and command bindings"); }
        const spRTTIRecord record{wxGolemAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxGolemAttackAIAction", &wxAIAction::StaticRTTI(), &CreateGolemAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxGolemAttackAIAction::wxGolemAttackAIAction(wxAIActionCommandExitHost& host, void* owner,
        const CapturedState& state, ProfileForAnalysis profile) noexcept : host_(host), state_(state), profile_(profile)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxGolemAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxGolemAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxGolemAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("GolemAttack Clone remains unqualified"); }
    bool wxGolemAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxGolemAttackAIAction::ResultForAnalysis wxGolemAttackAIAction::ExitForAnalysis() noexcept
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
        // The typed table selects shared5B79D0. Its original declaration
        // owner is unknown; the reset algorithm is the real common component.
        if ((*command)->ResetForAnalysis() != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        state_.borrowedTarget3A8 = nullptr;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    wxGolemAttackAIActionEntryResultForAnalysis wxGolemAttackAIAction::EnterForAnalysis() noexcept
    {
        using EntryResult = wxGolemAttackAIActionEntryResultForAnalysis;
        auto* const binding = entryBinding_;
        if (!binding || binding->profile != wxGolemAttackAIActionEntryProfileForAnalysis::PcCapturedPositiveRegistryNoEntity
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastEntryResult_ = EntryResult::UnqualifiedProfile;
        auto* const registry = binding->registry.existing;
        if (!registry) return lastEntryResult_ = EntryResult::MissingExistingRegistry;
        const auto selection = registry->SelectTargetForAnalysis();
        if (selection.result != wxCharacterRegistry::ResultForAnalysis::Completed)
            return lastEntryResult_ = EntryResult::RegistryNotAdmitted;
        // PC5B7BB3 publishes the real selector result before capturing owner.
        state_.borrowedTarget3A8 = selection.borrowedTarget;
        auto* const owner = GetOwnerForAnalysis();
        const auto storeWord = [this](std::size_t offset, std::uint32_t value) noexcept
        {
            for (unsigned byte = 0; byte != 4; ++byte)
                state_.ownBytes3AC[offset - 0x3AC + byte] = static_cast<std::uint8_t>(value >> (8 * byte));
        };
        storeWord(0x3AC, 0);
        storeWord(0x3B4, 0x481C4000);
        state_.ownBytes3AC[0x3BC - 0x3AC] = 0;
        storeWord(0x3B0, 0);
        // Native dependent owner24 read follows all five own stores. These
        // analytical refusals retain that prefix rather than rolling it back.
        if (!owner) return lastEntryResult_ = EntryResult::MissingOwner;
        auto* const storage = binding->ownerStorage;
        if (!storage) return lastEntryResult_ = EntryResult::MissingOwnerStorage;
        const auto entity = storage->ReadEntity24ForAnalysis(owner);
        if (!entity) return lastEntryResult_ = EntryResult::MissingOwnerStorage;
        // Notify precedes the target-null selector branch in the original.
        if (*entity) return lastEntryResult_ = EntryResult::NotifyNotAdmitted;
        if (!state_.borrowedTarget3A8) return lastEntryResult_ = EntryResult::SelectorNotAdmitted;
        return lastEntryResult_ = EntryResult::Completed;
    }
    void wxGolemAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxGolemAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxGolemAttackAIAction::RefuseHookForAnalysis() noexcept { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxGolemAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxGolemAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxGolemAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxGolemAttackAIAction::vfunc_30() noexcept
    {
        // Keep the existing Exit-result hook status separate from Entry.
        lastResult_ = ResultForAnalysis::UnqualifiedHook;
        (void)EnterForAnalysis();
    }
    void wxGolemAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxGolemAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxGolemAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
