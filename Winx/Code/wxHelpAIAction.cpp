#include "wxHelpAIAction.h"
#include <algorithm>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateHelp()
        { throw std::logic_error("Help requires explicit postconstruction owner/state bindings"); }
        const spRTTIRecord record{wxHelpAIAction::ClassID, wxAIAction::ClassID,
            "wxHelpAIAction", &wxAIAction::StaticRTTI(), &CreateHelp, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxHelpAIAction::wxHelpAIAction(wxHelpAIActionHost& host, void* owner,
        const wxHelpAIActionCapturedStateForAnalysis& state) noexcept
        : host_(host), state_(state)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxHelpAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxHelpAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxHelpAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Help Clone remains unqualified"); }
    bool wxHelpAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxHelpAIActionResultForAnalysis wxHelpAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxHelpAIActionResultForAnalysis;
        if (host_.EntryProfileForAnalysis() != wxHelpAIActionHost::ProfileForAnalysis::PcCapturedBorrowedEntryData
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        auto* const owner = GetOwnerForAnalysis();
        if (owner == nullptr) return lastResult_ = Result::MissingOwner;
        const auto command = host_.ReadCommandForAnalysis(owner);
        if (!command.has_value() || *command == nullptr)
            return lastResult_ = Result::MissingActorOrCommand;
        // PC5BAF81 writes the previously captured command, then its own byte
        // and word. Input rebinding during the command write cannot retarget it.
        host_.WriteCommandWordForAnalysis(*command, 4, 0);
        state_.ownBytes[0x3AC - 0x3A8] = 0;
        std::fill_n(state_.ownBytes.begin() + (0x460 - 0x3A8), 4, std::uint8_t{0});
        return lastResult_ = Result::Completed;
    }
    void wxHelpAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxHelpAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxHelpAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxHelpAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxHelpAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxHelpAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxHelpAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxHelpAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxHelpAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxHelpAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxHelpAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxHelpAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
