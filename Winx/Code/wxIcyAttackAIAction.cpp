#include "wxIcyAttackAIAction.h"
#include "wxCharacterRegistry.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateIcyAttack()
        { throw std::logic_error("Icy Entry requires explicit postconstruction state and an existing Registry binding"); }
        const spRTTIRecord record{wxIcyAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxIcyAttackAIAction", &wxAIAction::StaticRTTI(), &CreateIcyAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIcyAttackAIAction::wxIcyAttackAIAction(wxCharacterRegistryBindingForAnalysis& binding,
        const wxIcyAttackAIActionCapturedStateForAnalysis& state, ProfileForAnalysis profile) noexcept
        : binding_(binding), profile_(profile), state_(state) {}
    const spRTTIRecord& wxIcyAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIcyAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIcyAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Icy Clone remains unqualified"); }
    bool wxIcyAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxIcyAttackAIActionResultForAnalysis wxIcyAttackAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxIcyAttackAIActionResultForAnalysis;
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
        state_.borrowedTarget3A8 = selected.borrowedTarget;
        word(0x3B0, 0);
        word(0x3B8, 0);
        state_.ownBytes3AC[0] = 0;
        state_.ownBytes3AC[1] = 0;
        word(0x3C0, 0);
        word(0x3B4, 0);
        word(0x3C4, 0);
        word(0x410, 0);
        state_.ownBytes3AC[0x3E8 - 0x3AC] = 0;
        state_.ownBytes3AC[0x3E9 - 0x3AC] = 0;
        state_.ownBytes3AC[0x3EB - 0x3AC] = 0;
        word(0x3BC, 1);
        word(0x3C8, 2);
        state_.ownBytes3AC[0x468 - 0x3AC] = 1;
        return lastResult_ = Result::Completed;
    }
    wxIcyAttackAIActionExitResultForAnalysis wxIcyAttackAIAction::ExitForAnalysis() noexcept
    {
        using ExitResult = wxIcyAttackAIActionExitResultForAnalysis;
        lastResult_ = wxIcyAttackAIActionResultForAnalysis::UnqualifiedHook;
        if (!exitHost_ || exitHost_->ExitProfileForAnalysis() != wxIcyAttackAIActionExitHost::ProfileForAnalysis::PcCapturedStableCommandAndExistingProfile
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastExitResult_ = ExitResult::UnqualifiedProfile;
        auto* const owner = GetOwnerForAnalysis();
        if (!owner) return lastExitResult_ = ExitResult::MissingOwner;
        const auto actor = exitHost_->ReadActor144ForAnalysis(owner);
        if (!actor || !*actor) return lastExitResult_ = ExitResult::MissingActorStorage;
        const auto command = exitHost_->ReadCommand130ForAnalysis(*actor);
        if (!command || !*command) return lastExitResult_ = ExitResult::MissingCommandStorage;
        // Actual703090 slot04 ->5992E0 returns to5BE2D7. The shared
        // algorithm completes before the native existing-profile read.
        if ((*command)->ResetForAnalysis() != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastExitResult_ = ExitResult::CommandNotAdmitted;
        auto* const profileByte = exitHost_->ReadExistingProfileByte510ForAnalysis();
        if (!profileByte) return lastExitResult_ = ExitResult::MissingExistingProfile;
        *profileByte = 1;
        state_.borrowedTarget3A8 = nullptr;
        lastResult_ = wxIcyAttackAIActionResultForAnalysis::Completed;
        return lastExitResult_ = ExitResult::Completed;
    }
    void wxIcyAttackAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxIcyAttackAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxIcyAttackAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxIcyAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxIcyAttackAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxIcyAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxIcyAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxIcyAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxIcyAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxIcyAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxIcyAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxIcyAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
