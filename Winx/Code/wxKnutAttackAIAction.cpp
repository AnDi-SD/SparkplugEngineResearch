#include "wxKnutAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateKnutAttack()
        { throw std::logic_error("KnutAttack requires explicit captured state and live command storage"); }
        const spRTTIRecord record{wxKnutAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxKnutAttackAIAction", &wxAIAction::StaticRTTI(), &CreateKnutAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxKnutAttackAIAction::wxKnutAttackAIAction(const CapturedState& state,
        wxAIActionCommandExitHost* storage, ProfileForAnalysis profile) noexcept
        : state_(state), storage_(storage), profile_(profile) {}
    const spRTTIRecord& wxKnutAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxKnutAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxKnutAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("KnutAttack Clone remains unqualified"); }
    bool wxKnutAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxKnutAttackAIAction::ResultForAnalysis wxKnutAttackAIAction::ExitForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        using Command = wxAICommandRecordForAnalysis;
        using Store = Command::ResultForAnalysis (Command::*)(std::uint8_t) noexcept;
        // Concrete shared storage algorithms in the literal native order.
        // This is our operation list, not a native command vtable or callback.
        constexpr std::array<Store, 7> stores{{
            &Command::StoreByte1DForAnalysis,
            &Command::StoreByte5CForAnalysis,
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
    void wxKnutAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxKnutAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxKnutAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxKnutAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxKnutAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxKnutAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxKnutAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxKnutAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxKnutAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxKnutAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
