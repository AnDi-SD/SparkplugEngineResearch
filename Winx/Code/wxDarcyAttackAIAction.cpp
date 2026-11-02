#include "wxDarcyAttackAIAction.h"
#include "wxCharacterRegistry.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDarcyAttack()
        { throw std::logic_error("Darcy Entry requires explicit postconstruction state and an existing Registry binding"); }
        const spRTTIRecord record{wxDarcyAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxDarcyAttackAIAction", &wxAIAction::StaticRTTI(), &CreateDarcyAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDarcyAttackAIAction::wxDarcyAttackAIAction(wxCharacterRegistryBindingForAnalysis& binding,
        const wxDarcyAttackAIActionCapturedStateForAnalysis& state, ProfileForAnalysis profile) noexcept
        : binding_(binding), profile_(profile), state_(state) {}
    const spRTTIRecord& wxDarcyAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDarcyAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDarcyAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Darcy Clone remains unqualified"); }
    bool wxDarcyAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxDarcyAttackAIActionResultForAnalysis wxDarcyAttackAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxDarcyAttackAIActionResultForAnalysis;
        if (profile_ != ProfileForAnalysis::PcCapturedExistingRegistryEntry
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        auto* const registry = binding_.existing;
        if (!registry) return lastResult_ = Result::MissingExistingRegistry;
        const auto selected = registry->SelectTargetForAnalysis();
        if (selected.result != wxCharacterRegistry::ResultForAnalysis::Completed)
            return lastResult_ = Result::RegistryNotAdmitted;
        // Explicit PC-byte record writes. These do not impose the native ABI
        // on the portable C++ object or invent names for the remaining fields.
        const auto word = [this](std::size_t offset, std::uint32_t value) noexcept
        {
            for (unsigned i = 0; i != 4; ++i)
                state_.ownBytes3AC[offset - 0x3AC + i] = static_cast<std::uint8_t>(value >> (8 * i));
        };
        word(0x3B0, 0);
        word(0x3B8, 0);
        state_.ownBytes3AC[0] = 0;
        word(0x3C0, 0);
        word(0x3B4, 0);
        word(0x3CC, 0);
        word(0x400, 0);
        state_.ownBytes3AC[0x3F0 - 0x3AC] = 0;
        state_.ownBytes3AC[0x45C - 0x3AC] = 0;
        state_.borrowedTarget3A8 = selected.borrowedTarget;
        word(0x3BC, 1);
        word(0x3D0, 2);
        return lastResult_ = Result::Completed;
    }
    wxDarcyAttackAIActionExitResultForAnalysis wxDarcyAttackAIAction::ExitForAnalysis() noexcept
    {
        using ExitResult = wxDarcyAttackAIActionExitResultForAnalysis;
        lastResult_ = wxDarcyAttackAIActionResultForAnalysis::UnqualifiedHook;
        if (!exitHost_ || exitHost_->ExitProfileForAnalysis() != wxDarcyAttackAIActionExitHost::ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastExitResult_ = ExitResult::UnqualifiedProfile;
        // Original5C2230..5C229B reloads all three cells separately for
        // every byte store. Do not cache an earlier owner/actor/command.
        for (const auto offset : std::array<std::uint32_t, 6>{0x1D, 0x20, 0x21, 0x22, 0x23, 0x24})
        {
            auto* const owner = GetOwnerForAnalysis();
            if (!owner) return lastExitResult_ = ExitResult::MissingOwner;
            const auto actor = exitHost_->ReadActor144ForAnalysis(owner);
            if (!actor || !*actor) return lastExitResult_ = ExitResult::MissingActorStorage;
            const auto command = exitHost_->ReadCommand130ForAnalysis(*actor);
            if (!command || !*command) return lastExitResult_ = ExitResult::MissingCommandStorage;
            exitHost_->WriteCommandByteForAnalysis(*command, offset, 0);
        }
        // Original5C229E runs only after all six command writes complete.
        state_.borrowedTarget3A8 = nullptr;
        lastResult_ = wxDarcyAttackAIActionResultForAnalysis::Completed;
        return lastExitResult_ = ExitResult::Completed;
    }
    void wxDarcyAttackAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxDarcyAttackAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxDarcyAttackAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxDarcyAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxDarcyAttackAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxDarcyAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxDarcyAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxDarcyAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxDarcyAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxDarcyAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxDarcyAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxDarcyAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
