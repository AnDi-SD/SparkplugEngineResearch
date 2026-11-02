#include "wxStormyAttackAIAction.h"
#include "wxCharacterRegistry.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateStormyAttack()
        { throw std::logic_error("StormyAttack requires explicit postconstruction state and Registry binding"); }
        const spRTTIRecord record{wxStormyAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxStormyAttackAIAction", &wxAIAction::StaticRTTI(), &CreateStormyAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxStormyAttackAIAction::wxStormyAttackAIAction(wxCharacterRegistryBindingForAnalysis& binding, void* owner,
        const wxStormyAttackAIActionCapturedStateForAnalysis& state, wxStormyAttackAIActionProfileForAnalysis profile) noexcept
        : registryBinding_(binding), state_(state), profile_(profile)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxStormyAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxStormyAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxStormyAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("StormyAttack Clone remains unqualified"); }
    bool wxStormyAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    void wxStormyAttackAIAction::WriteOwnWordForAnalysis(std::size_t nativeOffset, std::uint32_t value) noexcept
    {
        for (unsigned byte = 0; byte != 4; ++byte)
            state_.ownBytes3AC[nativeOffset - 0x3AC + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
    }
    wxStormyAttackAIActionResultForAnalysis wxStormyAttackAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxStormyAttackAIActionResultForAnalysis;
        if (profile_ != wxStormyAttackAIActionProfileForAnalysis::PcExistingCapturedRegistry
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        // Read the storage binding afresh each call; it supplies the Registry,
        // while the actual common implementation performs target selection.
        auto* const registry = registryBinding_.existing;
        if (!registry) return lastResult_ = Result::MissingExistingRegistry;
        const auto selection = registry->SelectTargetForAnalysis();
        if (selection.result != wxCharacterRegistry::ResultForAnalysis::Completed)
            return lastResult_ = Result::RegistryNotAdmitted;
        // Original5C5710..5C5757: target first, then eleven literal stores.
        state_.borrowedTarget3A8 = selection.borrowedTarget;
        WriteOwnWordForAnalysis(0x3B0, 0);
        WriteOwnWordForAnalysis(0x3B8, 0);
        state_.ownBytes3AC[0] = 0;
        WriteOwnWordForAnalysis(0x3C4, 0);
        WriteOwnWordForAnalysis(0x3B4, 0);
        WriteOwnWordForAnalysis(0x3C8, 0);
        WriteOwnWordForAnalysis(0x3D4, 0);
        state_.ownBytes3AC[0x3F4 - 0x3AC] = 0;
        WriteOwnWordForAnalysis(0x3C0, 1);
        state_.ownBytes3AC[0x3D0 - 0x3AC] = 1;
        WriteOwnWordForAnalysis(0x3D8, 2);
        return lastResult_ = Result::Completed;
    }
    wxStormyAttackAIActionExitResultForAnalysis wxStormyAttackAIAction::ExitForAnalysis() noexcept
    {
        using ExitResult = wxStormyAttackAIActionExitResultForAnalysis;
        lastResult_ = wxStormyAttackAIActionResultForAnalysis::UnqualifiedHook;
        if (!exitHost_ || exitHost_->ExitProfileForAnalysis() != wxStormyAttackAIActionExitHost::ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastExitResult_ = ExitResult::UnqualifiedProfile;
        auto* const owner = GetOwnerForAnalysis();
        if (!owner) return lastExitResult_ = ExitResult::MissingOwner;
        const auto actor = exitHost_->ReadActor144ForAnalysis(owner);
        if (!actor || !*actor) return lastExitResult_ = ExitResult::MissingActorStorage;
        const auto command = exitHost_->ReadCommand130ForAnalysis(*actor);
        if (!command || !*command) return lastExitResult_ = ExitResult::MissingCommandStorage;
        // Actual table703090 slot04 calls5992E0, returning to5B79E7.
        // Invoke the common recovered algorithm, never a host reset callback.
        if ((*command)->ResetForAnalysis() != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastExitResult_ = ExitResult::CommandNotAdmitted;
        state_.borrowedTarget3A8 = nullptr;
        lastResult_ = wxStormyAttackAIActionResultForAnalysis::Completed;
        return lastExitResult_ = ExitResult::Completed;
    }
    void wxStormyAttackAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxStormyAttackAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxStormyAttackAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxStormyAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxStormyAttackAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxStormyAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxStormyAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxStormyAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxStormyAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxStormyAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxStormyAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxStormyAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
