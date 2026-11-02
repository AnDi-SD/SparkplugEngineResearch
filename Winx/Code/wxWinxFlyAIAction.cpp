#include "wxWinxFlyAIAction.h"
#include "wxCharacterRegistry.h"
#include <algorithm>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateWinxFly()
        { throw std::logic_error("WinxFly requires explicit postconstruction owner/state bindings"); }
        const spRTTIRecord record{wxWinxFlyAIAction::ClassID, wxAIAction::ClassID,
            "wxWinxFlyAIAction", &wxAIAction::StaticRTTI(), &CreateWinxFly, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxWinxFlyAIAction::wxWinxFlyAIAction(wxWinxFlyAIActionHost& host, void* owner,
        const wxWinxFlyAIActionCapturedStateForAnalysis& state) noexcept : host_(host), state_(state)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxWinxFlyAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxWinxFlyAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxWinxFlyAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("WinxFly Clone remains unqualified"); }
    bool wxWinxFlyAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxWinxFlyAIActionResultForAnalysis wxWinxFlyAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxWinxFlyAIActionResultForAnalysis;
        if (host_.EntryProfileForAnalysis() != wxWinxFlyAIActionHost::ProfileForAnalysis::PcCapturedExistingRegistryEntry
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        void* const owner = GetOwnerForAnalysis();
        if (!owner) return lastResult_ = Result::MissingOwner;
        const auto command = host_.ReadCommandForAnalysis(owner);
        if (!command.has_value()) return lastResult_ = Result::MissingActorOrCommandStorage;
        if (*command) host_.WriteCommandWordForAnalysis(*command, 4, 0);
        // The native binding is read only after the nullable command branch.
        auto* const registry = host_.ReadExistingRegistryForAnalysis();
        if (!registry) return lastResult_ = Result::MissingExistingRegistry;
        const auto selection = registry->SelectTargetForAnalysis();
        if (selection.result != wxCharacterRegistry::ResultForAnalysis::Completed)
            return lastResult_ = Result::RegistryNotAdmitted;
        // Original5BC6D6..5BC6F4: preserve the explicit zero/target store order.
        std::fill_n(state_.ownBytes3AC.begin(), 4, std::uint8_t{0});
        std::fill_n(state_.ownBytes3AC.begin() + 0x0C, 4, std::uint8_t{0});
        state_.ownBytes3AC[0x10] = 0;
        std::fill_n(state_.ownBytes3AC.begin() + 4, 4, std::uint8_t{0});
        std::fill_n(state_.ownBytes3AC.begin() + 8, 4, std::uint8_t{0});
        state_.borrowedTarget3A8 = selection.borrowedTarget;
        return lastResult_ = Result::Completed;
    }
    void wxWinxFlyAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxWinxFlyAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxWinxFlyAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxWinxFlyAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxWinxFlyAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxWinxFlyAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxWinxFlyAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxWinxFlyAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxWinxFlyAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxWinxFlyAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxWinxFlyAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxWinxFlyAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
