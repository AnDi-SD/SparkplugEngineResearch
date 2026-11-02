#include "wxTrollAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTrollAttack()
        { throw std::logic_error("TrollAttack requires explicit captured state and live command storage"); }
        const spRTTIRecord record{wxTrollAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxTrollAttackAIAction", &wxAIAction::StaticRTTI(), &CreateTrollAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxTrollAttackAIAction::wxTrollAttackAIAction(const CapturedState& state,
        wxAIActionCommandExitHost* storage, ProfileForAnalysis profile) noexcept
        : state_(state), storage_(storage), profile_(profile) {}
    const spRTTIRecord& wxTrollAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxTrollAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxTrollAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("TrollAttack Clone remains unqualified"); }
    bool wxTrollAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxTrollAttackAIAction::ResultForAnalysis wxTrollAttackAIAction::ExitForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        using Command = wxAICommandRecordForAnalysis;
        using Store = Command::ResultForAnalysis (Command::*)(std::uint8_t) noexcept;
        // Concrete shared storage algorithms in the literal native order.
        // This is our operation list, not a native command vtable or callback.
        constexpr std::array<Store, 6> stores{{
            &Command::StoreByte1DForAnalysis,
            &Command::StoreByte20ForAnalysis,
            &Command::StoreByte21ForAnalysis,
            &Command::StoreByte22ForAnalysis,
            &Command::StoreByte23ForAnalysis,
            &Command::StoreByte24ForAnalysis
        }};
        for (auto store : stores)
        {
            // Every original flag store starts a fresh own20/owner144/actor130
            // chain. Do not hoist the binding across an earlier byte store.
            auto* const owner = GetOwnerForAnalysis();
            if (!owner) return lastResult_ = ResultForAnalysis::MissingOwner;
            if (!storage_) return lastResult_ = ResultForAnalysis::MissingActorStorage;
            const auto actor = storage_->ReadActor144ForAnalysis(owner);
            if (!actor || !*actor) return lastResult_ = ResultForAnalysis::MissingActorStorage;
            const auto command = storage_->ReadCommand130ForAnalysis(*actor);
            if (!command || !*command) return lastResult_ = ResultForAnalysis::MissingCommandStorage;
            if (((*command)->*store)(0) != Command::ResultForAnalysis::Completed)
                return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        }
        state_.borrowedTarget3A8 = nullptr;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    void wxTrollAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxTrollAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxTrollAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxTrollAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxTrollAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxTrollAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxTrollAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxTrollAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxTrollAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxTrollAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
