#include "wxAttackAIAction.h"

#include <initializer_list>

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        wxAttackAIActionHost* factoryHost = nullptr;

        std::unique_ptr<spBaseObject> CreateAttackAIAction()
        {
            if (factoryHost == nullptr) return nullptr;
            return std::make_unique<wxAttackAIAction>(*factoryHost);
        }

        const spRTTIRecord AttackRecord{
            wxAttackAIAction::ClassID,
            wxAIAction::ClassID,
            "wxAttackAIAction",
            &wxAIAction::StaticRTTI(),
            &CreateAttackAIAction,
            nullptr,
        };

        const bool AttackRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(AttackRecord);
    }

    wxAttackAIAction::wxAttackAIAction(wxAttackAIActionHost& host) noexcept
        : host_(host) {}

    void wxAttackAIAction::SetFactoryHostForAnalysis(
        wxAttackAIActionHost* const host) noexcept
    {
        factoryHost = host;
    }

    const spRTTIRecord& wxAttackAIAction::StaticRTTI() noexcept
    {
        (void)AttackRegistered;
        return AttackRecord;
    }

    void wxAttackAIAction::vfunc_0C(const void* const notification) noexcept
    {
        const auto* const message =
            static_cast<const wxAIActionMessageForAnalysis*>(notification);
        if (message->code == 0x273B)
        {
            host_.DispatchSlotForAnalysis(*this, 17, notification);
        }
        else if (message->code != 0x2754)
        {
            wxAIAction::vfunc_0C(notification);
        }
    }

    std::unique_ptr<spBaseObject> wxAttackAIAction::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxAttackAIAction>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxAttackAIAction::vfunc_14(
        spBaseObject& destination, spCloneManager& manager) const
    {
        if (!destination.IsExactly(ClassID)) return false;
        if (!wxAIAction::vfunc_14(destination, manager)) return false;
        auto& attack = static_cast<wxAttackAIAction&>(destination);
        // Original Copy transfers only these four words. In particular, it
        // leaves +3B8, the inline PathFinder and +3F0 untouched.
        attack.target_ = target_;
        attack.state_ = state_;
        attack.deadline_ = deadline_;
        attack.field3B4_ = field3B4_;
        return true;
    }

    const spRTTIRecord& wxAttackAIAction::vfunc_18() const noexcept
    {
        return AttackRecord;
    }

    bool wxAttackAIAction::vfunc_24() noexcept
    {
        // PC 5AA1D0 / PS2 243DA0. Every state branch returns true after its
        // virtual handler; out-of-range states return true directly.
        static constexpr std::uint32_t slots[7] = {23, 18, 19, 20, 21, 22, 24};
        if (state_ < 7) host_.DispatchSlotForAnalysis(*this, slots[state_], nullptr);
        return true;
    }

    void wxAttackAIAction::vfunc_30() noexcept
    {
        // PC 5AA3B0 / PS2 243C60. The original method receives an unused
        // argument; the portable base signature omits it.
        host_.SetCommandByteForAnalysis(0x1D, 1);
        if (host_.GetOwnerFlag1C5ForAnalysis())
        {
            target_ = host_.FindRegistryTargetForAnalysis();
            state_ = 0;
            deadline_ = 0;
        }
        else
        {
            target_ = host_.FindNearestTargetForAnalysis();
            state_ = 4;
            const auto base = host_.GetTimerBaseForAnalysis();
            const auto maximum = host_.GetOwnerMaxForAnalysis();
            const auto minimum = host_.GetOwnerMinForAnalysis();
            const auto random = host_.NextRandomForAnalysis();
            deadline_ = base + minimum + random % (maximum - minimum + 1u);
        }
        field3B8_ = 0;
        if (target_ == nullptr) host_.SelectActionForAnalysis(1, 0);
        // These writes follow the selector call in the original. If a host
        // implements a non-returning transition, they are not reached either.
        SetDurationMillisecondsForAnalysis(2000);
        flag3F0_ = 0;
    }

    void wxAttackAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        for (const auto offset : {0x1Du, 0x20u, 0x21u, 0x1Fu})
            host_.SetCommandByteForAnalysis(offset, 0);
        target_ = nullptr;
    }

    void wxAttackAIAction::vfunc_38() noexcept
    {
        if (host_.FindNearestTargetForAnalysis() == nullptr
            && !host_.GetOwnerFlag1C5ForAnalysis())
        {
            host_.SelectActionForAnalysis(0, 0);
        }
    }

    void wxAttackAIAction::SetOwnFieldsForAnalysis(void* const target,
        const std::uint32_t state, const std::uint32_t deadline,
        const std::uint32_t field3B4, const std::uint32_t field3B8,
        const std::uint8_t flag3F0) noexcept
    {
        target_ = target;
        state_ = state;
        deadline_ = deadline;
        field3B4_ = field3B4;
        field3B8_ = field3B8;
        flag3F0_ = flag3F0;
    }
}
