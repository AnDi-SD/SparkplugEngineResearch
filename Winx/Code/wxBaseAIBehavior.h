#pragma once

#include "Code/SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>
#include <map>

namespace winx::reconstruction
{
    class wxAIAction;

    // Portable bridge for the original action v10 entry parameter. The
    // reconstructed wxAIAction::vfunc_30 currently has no argument.
    class wxBaseAIBehaviorActionBridge
    {
    public:
        virtual ~wxBaseAIBehaviorActionBridge() = default;
        virtual void EnterForAnalysis(wxAIAction&, std::uint32_t parameter) noexcept = 0;
    };

    // Native physical parent is wxEntity. The missing entity implementation
    // is represented by spNamedObject plus the verified RTTI chain.
    class wxBaseAIBehavior : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x457334F1;

        wxBaseAIBehavior() noexcept = default;
        ~wxBaseAIBehavior() override = default;
        wxBaseAIBehavior(const wxBaseAIBehavior&) = delete;
        wxBaseAIBehavior& operator=(const wxBaseAIBehavior&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;

        // Explicitly borrowed action bindings. Native map insertion and
        // ownership outside the tested selector are not reconstructed.
        void BindActionForAnalysis(std::uint32_t key, wxAIAction* action,
            std::uint32_t nativeActionKey);
        void BindActionForAnalysis(std::uint32_t key, wxAIAction& action,
            std::uint32_t nativeActionKey)
        {
            BindActionForAnalysis(key, &action, nativeActionKey);
        }
        [[nodiscard]] wxAIAction* FindActionForAnalysis(std::uint32_t key) const noexcept;
        [[nodiscard]] bool SelectActionForAnalysis(std::uint32_t key,
            std::uint32_t parameter) noexcept;
        void SetActionGateForAnalysis(std::uint32_t value) noexcept;
        void SetActionBridgeForAnalysis(wxBaseAIBehaviorActionBridge* bridge) noexcept;
        void SetGateControlsForAnalysis(bool changeEnabled, bool inhibitSwitch) noexcept;

        [[nodiscard]] wxAIAction* GetCurrentActionForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetSavedActionKeyForAnalysis() const noexcept;
        [[nodiscard]] std::uint8_t GetActionGateForAnalysis() const noexcept;
        [[nodiscard]] bool GetGateChangeEnabledForAnalysis() const noexcept;
        [[nodiscard]] bool GetSwitchInhibitedForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetActionCountForAnalysis() const noexcept;
        [[nodiscard]] const std::array<float, 3>&
            GetBaseValuesForAnalysis() const noexcept;

    protected:
        void SwitchToActionForAnalysis(wxAIAction* next,
            std::uint32_t parameter) noexcept;
        // Default clone is verified only for an empty action map.
        [[nodiscard]] bool CopyEmptyBaseForAnalysis(wxBaseAIBehavior& destination,
            sparkplug::reconstruction::spCloneManager& manager) const;

    private:
        struct ActionBinding final
        {
            wxAIAction* action; // borrowed
            std::uint32_t nativeActionKey;
        };
        std::map<std::uint32_t, ActionBinding> actions_;
        wxAIAction* currentAction_ = nullptr;
        std::uint32_t savedActionKey_ = 0;
        std::uint8_t actionGate_ = 0;
        bool changeGateEnabled_ = true;
        bool inhibitSwitch_ = false;
        std::array<float, 3> baseValues_{125.0f, 400.0f, 700.0f};
        wxBaseAIBehaviorActionBridge* bridge_ = nullptr; // borrowed adapter
    };
}
