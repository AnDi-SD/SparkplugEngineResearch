#include "wxBaseAIBehavior.h"

#include "Code/wxAIAction.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        const spRTTIRecord spEntityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &spEntityRecord, nullptr, nullptr};
        const spRTTIRecord behaviorRecord{wxBaseAIBehavior::ClassID, 0x796A1869,
            "wxBaseAIBehavior", &wxEntityRecord, nullptr, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(
            behaviorRecord);
    }

    const spRTTIRecord& wxBaseAIBehavior::StaticRTTI() noexcept
    {
        (void)registered;
        return behaviorRecord;
    }

    const spRTTIRecord& wxBaseAIBehavior::vfunc_18() const noexcept
    {
        return behaviorRecord;
    }

    void wxBaseAIBehavior::BindActionForAnalysis(const std::uint32_t key,
        wxAIAction* const action, const std::uint32_t nativeActionKey)
    {
        actions_.insert_or_assign(key, ActionBinding{action, nativeActionKey});
    }

    wxAIAction* wxBaseAIBehavior::FindActionForAnalysis(
        const std::uint32_t key) const noexcept
    {
        const auto found = actions_.find(key); // uint32_t ordering is unsigned
        return found == actions_.end() ? nullptr : found->second.action;
    }

    bool wxBaseAIBehavior::SelectActionForAnalysis(const std::uint32_t key,
        const std::uint32_t parameter) noexcept
    {
        if (actionGate_ != 0 && key != 0)
        {
            return false;
        }
        auto* next = FindActionForAnalysis(key);
        if (next == nullptr)
        {
            next = FindActionForAnalysis(0);
        }
        SwitchToActionForAnalysis(next, parameter);
        return true; // original accepts even when no fallback exists
    }

    void wxBaseAIBehavior::SwitchToActionForAnalysis(wxAIAction* const next,
        const std::uint32_t parameter) noexcept
    {
        if (currentAction_ != nullptr)
        {
            currentAction_->vfunc_34_ClearForAnalysis();
        }
        currentAction_ = next;
        if (next != nullptr)
        {
            if (bridge_ != nullptr)
            {
                bridge_->EnterForAnalysis(*next, parameter);
            }
            else
            {
                next->vfunc_30();
            }
        }
    }

    void wxBaseAIBehavior::SetActionGateForAnalysis(
        const std::uint32_t value) noexcept
    {
        const auto gate = static_cast<std::uint8_t>(value);
        if (!changeGateEnabled_ || gate == actionGate_)
        {
            return;
        }
        actionGate_ = gate;
        if (currentAction_ == nullptr || inhibitSwitch_)
        {
            return;
        }
        if (gate != 0)
        {
            // The original reads action+28/+2c, which is independent of the
            // map key. Bindings retain that observed value explicitly.
            for (const auto& [key, binding] : actions_)
            {
                (void)key;
                if (binding.action == currentAction_)
                {
                    savedActionKey_ = binding.nativeActionKey;
                    break;
                }
            }
        }
        (void)SelectActionForAnalysis(gate != 0 ? 0 : savedActionKey_, 0);
    }

    void wxBaseAIBehavior::SetActionBridgeForAnalysis(
        wxBaseAIBehaviorActionBridge* const bridge) noexcept
    {
        bridge_ = bridge;
    }

    void wxBaseAIBehavior::SetGateControlsForAnalysis(const bool changeEnabled,
        const bool inhibitSwitch) noexcept
    {
        changeGateEnabled_ = changeEnabled;
        inhibitSwitch_ = inhibitSwitch;
    }

    wxAIAction* wxBaseAIBehavior::GetCurrentActionForAnalysis() const noexcept
    {
        return currentAction_;
    }
    std::uint32_t wxBaseAIBehavior::GetSavedActionKeyForAnalysis() const noexcept
    {
        return savedActionKey_;
    }
    std::uint8_t wxBaseAIBehavior::GetActionGateForAnalysis() const noexcept
    {
        return actionGate_;
    }
    bool wxBaseAIBehavior::GetGateChangeEnabledForAnalysis() const noexcept
    {
        return changeGateEnabled_;
    }
    bool wxBaseAIBehavior::GetSwitchInhibitedForAnalysis() const noexcept
    {
        return inhibitSwitch_;
    }
    std::size_t wxBaseAIBehavior::GetActionCountForAnalysis() const noexcept
    {
        return actions_.size();
    }
    const std::array<float, 3>&
        wxBaseAIBehavior::GetBaseValuesForAnalysis() const noexcept
    {
        return baseValues_;
    }

    bool wxBaseAIBehavior::CopyEmptyBaseForAnalysis(
        wxBaseAIBehavior& destination, spCloneManager& manager) const
    {
        if (!actions_.empty())
        {
            return false;
        }
        destination.actions_.clear();
        destination.currentAction_ = nullptr;
        destination.savedActionKey_ = 0;
        destination.actionGate_ = 0;
        destination.changeGateEnabled_ = true;
        destination.inhibitSwitch_ = false;
        destination.baseValues_ = {125.0f, 400.0f, 700.0f};
        destination.bridge_ = nullptr;
        return spNamedObject::vfunc_14(destination, manager);
    }
}
