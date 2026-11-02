#include "wxFrogAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFrogAttack()
        { throw std::logic_error("FrogAttack requires explicit captured state and cached command"); }
        const spRTTIRecord record{wxFrogAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxFrogAttackAIAction", &wxAIAction::StaticRTTI(), &CreateFrogAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFrogAttackAIAction::wxFrogAttackAIAction(const CapturedState& state,
        ProfileForAnalysis profile) noexcept : state_(state), profile_(profile) {}
    const spRTTIRecord& wxFrogAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFrogAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFrogAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("FrogAttack Clone remains unqualified"); }
    bool wxFrogAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxFrogAttackAIAction::ResultForAnalysis wxFrogAttackAIAction::ExitForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCapturedStableCachedCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        auto* command = state_.borrowedCachedCommand3E8;
        if (!command) return lastResult_ = ResultForAnalysis::MissingCachedCommand;
        if (command->StoreByte1DForAnalysis(0) != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        // PC5B3EDB reloads the cache after the first command store.
        command = state_.borrowedCachedCommand3E8;
        if (!command) return lastResult_ = ResultForAnalysis::MissingCachedCommand;
        if (command->StoreByte20ForAnalysis(0) != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        state_.borrowedTarget3A8 = nullptr;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    void wxFrogAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxFrogAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxFrogAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxFrogAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxFrogAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxFrogAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxFrogAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxFrogAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxFrogAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxFrogAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
