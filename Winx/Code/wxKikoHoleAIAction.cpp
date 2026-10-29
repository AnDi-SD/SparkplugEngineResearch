#include "wxKikoHoleAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include <cstring>
#include <typeinfo>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    using Result = wxKikoHoleResultForAnalysis;
    namespace
    {
        const spRTTIRecord record{wxKikoHoleAIAction::ClassID, wxAIAction::ClassID,
            "wxKikoHoleAIAction", &wxAIAction::StaticRTTI(), nullptr, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        std::uint32_t Word(const void* data, std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, static_cast<const std::uint8_t*>(data) + offset, sizeof value);
            return value;
        }
        // Host admission only. No derived callbacks, cyclic/unbounded input,
        // or graph outside the supplied four-plain-Node subtree is admitted.
        bool PlainSubtree(const spNode& node, unsigned& count) noexcept
        {
            if (++count > 4 || typeid(node) != typeid(spNode)) return false;
            for (std::size_t i = 0; i != node.GetChildCountForAnalysis(); ++i)
            {
                const auto* child = node.GetChildForAnalysis(i);
                if (!child || child->GetParentForAnalysis() != &node || !PlainSubtree(*child, count)) return false;
            }
            return true;
        }
    }
    wxKikoHoleAIAction::wxKikoHoleAIAction(wxKikoHoleAIActionHost& host,
        const wxKikoHoleStateForAnalysis& captured) noexcept : host_(host), state_(captured) {}
    const spRTTIRecord& wxKikoHoleAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxKikoHoleAIAction::vfunc_18() const noexcept { return record; }
    // Unsupported lifecycle operations refuse explicitly rather than returning
    // a base-only clone or silently inventing protected constructor behavior.
    std::unique_ptr<spBaseObject> wxKikoHoleAIAction::vfunc_10(spCloneManager&) const { return {}; }
    bool wxKikoHoleAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    bool wxKikoHoleAIAction::vfunc_2C() noexcept
    { lastResult_ = Result::UnqualifiedEntry; return false; }
    void wxKikoHoleAIAction::vfunc_0C(const void* notification) noexcept
    {
        lastResult_ = Result::Completed;
        const auto code = Word(notification, 0);
        if (code == 0x2738)
        {
            if (static_cast<const std::uint8_t*>(notification)[0x1C])
            {
                state_.targetWord3B0 = Word(notification, 0x18);
                state_.phase3B4 = 2;
            }
            else state_.phase3B4 = 3;
            state_.once3B8 = 0;
        }
        else if (code == 0x2721 && Word(notification, 0x18) == 0x19)
        {
            state_.phase3B4 = 1;
            if (!state_.ownNode) { lastResult_ = Result::UnqualifiedNode; return; }
            state_.ownNode->SetEnabledForAnalysis(false, true);
        }
        // Unknown messages do not forward to wxAIAction.
    }
    Result wxKikoHoleAIAction::UpdateForAnalysis() noexcept
    {
        if (state_.phase3B4 == 2 && !state_.once3B8)
        {
            // Native522220 skips detach/reparent/parent Update when parent2C
            // equals target3B0. Preserve the accepted null/null branch; admit
            // a nonzero identity only through a separately supplied binding.
            if (!state_.ownNode) return lastResult_ = Result::UnqualifiedPhase2;
            const auto* currentParent = state_.ownNode->GetParentForAnalysis();
            if (state_.targetWord3B0 == 0)
            {
                if (currentParent) return lastResult_ = Result::UnqualifiedPhase2;
            }
            else
            {
                const auto& binding = state_.equalParentBinding;
                unsigned count = 0;
                if (!binding.nativeWord || binding.nativeWord != state_.targetWord3B0
                    || !binding.parent || currentParent != binding.parent
                    || typeid(*binding.parent) != typeid(spNode)
                    || binding.parent->GetParentForAnalysis() != nullptr
                    || binding.parent->GetChildCountForAnalysis() != 1
                    || binding.parent->GetChildForAnalysis(0) != state_.ownNode
                    || !PlainSubtree(*state_.ownNode, count))
                    return lastResult_ = Result::UnqualifiedPhase2;
            }
            state_.ownNode->SetEnabledForAnalysis(true, true);
            auto* command = host_.ReadCommandForAnalysis(GetOwnerForAnalysis());
            if (!command) return lastResult_ = Result::UnqualifiedCommand;
            host_.WriteCommandByteForAnalysis(command, 0x1B, 1);
            state_.once3B8 = 1;
        }
        else if (state_.phase3B4 == 1 && !state_.once3B8)
        {
            if (!state_.ownNode) return lastResult_ = Result::UnqualifiedNode;
            state_.ownNode->SetEnabledForAnalysis(false, true);
            state_.once3B8 = 1;
        }
        else if (state_.phase3B4 == 3 && !state_.once3B8)
        {
            auto* command = host_.ReadCommandForAnalysis(GetOwnerForAnalysis());
            if (!command) return lastResult_ = Result::UnqualifiedCommand;
            host_.WriteCommandByteForAnalysis(command, 0x1B, 0);
            state_.once3B8 = 1;
        }
        return lastResult_ = Result::Completed;
    }
    bool wxKikoHoleAIAction::vfunc_24() noexcept { return UpdateForAnalysis() == Result::Completed; }
}
