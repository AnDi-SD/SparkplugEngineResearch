#include "wxGhoulScriptAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include <stdexcept>
#include <typeinfo>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateGhoulScript()
        { throw std::logic_error("GhoulScript requires explicit postconstruction owner/state bindings"); }
        const spRTTIRecord record{wxGhoulScriptAIAction::ClassID, wxAIAction::ClassID,
            "wxGhoulScriptAIAction", &wxAIAction::StaticRTTI(), &CreateGhoulScript, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxGhoulScriptAIAction::wxGhoulScriptAIAction(wxGhoulScriptAIActionHost& host,
        void* owner, const wxGhoulScriptCapturedStateForAnalysis& state) noexcept
        : host_(host), state_(state)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxGhoulScriptAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxGhoulScriptAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxGhoulScriptAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("GhoulScript Clone remains unqualified"); }
    bool wxGhoulScriptAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxGhoulScriptResultForAnalysis wxGhoulScriptAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxGhoulScriptResultForAnalysis;
        if (host_.EntryProfileForAnalysis() != wxGhoulScriptAIActionHost::ProfileForAnalysis::PcCapturedBorrowedEntryData
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0
            || state_.opaqueNativeWord3B4 != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        // Original EAX caches owner ONCE, including the later owner14D write.
        auto* const owner = GetOwnerForAnalysis();
        if (owner == nullptr) return lastResult_ = Result::MissingOwner;
        const auto node = host_.ReadOwnerNodeForAnalysis(owner);
        if (!node.has_value()) return lastResult_ = Result::MissingEntity;
        if (*node != nullptr && typeid(**node) != typeid(spNode))
            return lastResult_ = Result::UnqualifiedNodeType;
        state_.borrowedNode3B0 = *node;
        // This read starts at the declared5A993F input boundary; the preceding
        // Node cache survives an analytical missing-actor refusal.
        const auto command = host_.ReadCommandForAnalysis(owner);
        if (!command.has_value()) return lastResult_ = Result::MissingActor;
        state_.borrowedCommand3B8 = *command;
        host_.WriteOwnerByteForAnalysis(owner, 0x14D, 0);
        state_.state3A8 = 0;
        state_.word3AC = 0;
        SetDurationMillisecondsForAnalysis(15000);
        return lastResult_ = Result::Completed;
    }
    void wxGhoulScriptAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxGhoulScriptAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxGhoulScriptResultForAnalysis::UnqualifiedNotify; }
    bool wxGhoulScriptAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxGhoulScriptResultForAnalysis::UnqualifiedHook; return false; }
    bool wxGhoulScriptAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxGhoulScriptAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxGhoulScriptAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxGhoulScriptAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxGhoulScriptAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxGhoulScriptAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxGhoulScriptAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
