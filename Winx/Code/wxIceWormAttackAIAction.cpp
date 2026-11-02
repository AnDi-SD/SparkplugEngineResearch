#include "wxIceWormAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateIceWormAttack()
        { throw std::logic_error("IceWormAttack requires explicit captured state and live command storage"); }
        const spRTTIRecord record{wxIceWormAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxIceWormAttackAIAction", &wxAIAction::StaticRTTI(), &CreateIceWormAttack, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceWormAttackAIAction::wxIceWormAttackAIAction(const CapturedState& state,
        wxAIActionCommandExitHost* storage, ProfileForAnalysis profile) noexcept
        : state_(state), storage_(storage), profile_(profile) {}
    const spRTTIRecord& wxIceWormAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceWormAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceWormAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("IceWormAttack Clone remains unqualified"); }
    bool wxIceWormAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxIceWormAttackAIAction::ResultForAnalysis wxIceWormAttackAIAction::ExitForAnalysis() noexcept
    {
        if (profile_ != ProfileForAnalysis::PcCapturedStableCommand
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        for (unsigned store = 0; store != 2; ++store)
        {
            // PC5ADB90/5ADBA4 each start a fresh own20->owner144->actor130
            // chain. Do not hoist this binding across the first byte store.
            auto* const owner = GetOwnerForAnalysis();
            if (!owner) return lastResult_ = ResultForAnalysis::MissingOwner;
            if (!storage_) return lastResult_ = ResultForAnalysis::MissingActorStorage;
            const auto actor = storage_->ReadActor144ForAnalysis(owner);
            if (!actor || !*actor) return lastResult_ = ResultForAnalysis::MissingActorStorage;
            const auto command = storage_->ReadCommand130ForAnalysis(*actor);
            if (!command || !*command) return lastResult_ = ResultForAnalysis::MissingCommandStorage;
            const auto result = store == 0 ? (*command)->StoreByte1DForAnalysis(0)
                : (*command)->StoreByte20ForAnalysis(0);
            if (result != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
                return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
        }
        state_.borrowedTarget3A8 = nullptr;
        return lastResult_ = ResultForAnalysis::Completed;
    }
    void wxIceWormAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)ExitForAnalysis(); }
    void wxIceWormAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedNotify; }
    bool wxIceWormAttackAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxIceWormAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxIceWormAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxIceWormAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxIceWormAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxIceWormAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxIceWormAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxIceWormAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
