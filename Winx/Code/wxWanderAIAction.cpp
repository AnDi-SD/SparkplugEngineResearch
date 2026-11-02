#include "wxWanderAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include <cstring>
#include <stdexcept>
#include <typeinfo>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateWander()
        { throw std::logic_error("Wander requires explicit postconstruction owner/state bindings"); }
        const spRTTIRecord record{wxWanderAIAction::ClassID, wxAIAction::ClassID,
            "wxWanderAIAction", &wxAIAction::StaticRTTI(), &CreateWander, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        std::uint32_t Bits(const float& value) noexcept
        { std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
    }
    wxWanderAIAction::wxWanderAIAction(wxWanderAIActionHost& host, void* owner,
        const wxWanderCapturedStateForAnalysis& state) noexcept : host_(host), state_(state)
    { SetOwnerForAnalysis(owner); }
    const spRTTIRecord& wxWanderAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxWanderAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxWanderAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Wander Clone remains unqualified"); }
    bool wxWanderAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxWanderResultForAnalysis wxWanderAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxWanderResultForAnalysis;
        if (host_.EntryProfileForAnalysis() != wxWanderAIActionHost::ProfileForAnalysis::PcCapturedBorrowedPosition
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        // Both stores precede the first owner read. Analytical later refusals
        // preserve these effects; they do not claim native null/fault semantics.
        state_.nativeTailWords3A8[4] = 0; // native3B8
        state_.nativeTailWords3A8[5] = 0; // native3BC
        void* const owner = GetOwnerForAnalysis();
        if (owner == nullptr) return lastResult_ = Result::MissingOwner;
        const auto node = host_.ReadOwnerNodeForAnalysis(owner);
        if (!node.has_value()) return lastResult_ = Result::MissingEntity;
        if (*node == nullptr) return lastResult_ = Result::MissingNode;
        if (typeid(**node) != typeid(spNode)) return lastResult_ = Result::UnqualifiedNodeType;
        const auto& position = (*node)->GetPositionForAnalysis();
        const std::array<std::uint32_t, 3> words{Bits(position[0]), Bits(position[1]), Bits(position[2])};
        // X/Y execute FLD/FSTP in the original; the admitted profile is finite
        // binary32 under the captured x87 environment. Z is an integer copy.
        if ((words[0] & 0x7F800000u) == 0x7F800000u || (words[1] & 0x7F800000u) == 0x7F800000u)
            return lastResult_ = Result::UnqualifiedPosition;
        state_.nativeTailWords3A8[0] = words[0];
        state_.nativeTailWords3A8[2] = words[2];
        state_.nativeTailWords3A8[1] = words[1];
        return lastResult_ = Result::Completed;
    }
    void wxWanderAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    void wxWanderAIAction::vfunc_0C(const void*) noexcept
    { lastResult_ = wxWanderResultForAnalysis::UnqualifiedNotify; }
    bool wxWanderAIAction::RefuseHookForAnalysis() noexcept
    { lastResult_ = wxWanderResultForAnalysis::UnqualifiedHook; return false; }
    bool wxWanderAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxWanderAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxWanderAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxWanderAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxWanderAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxWanderAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxWanderAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
