#include "wxBacoAttackAIAction.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        wxBacoAttackAIActionHost* factoryHost = nullptr;

        std::unique_ptr<spBaseObject> CreateBacoAttackAIAction()
        {
            if (factoryHost == nullptr) return nullptr;
            return std::make_unique<wxBacoAttackAIAction>(*factoryHost);
        }

        const spRTTIRecord BacoAttackRecord{
            wxBacoAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxBacoAttackAIAction", &wxAIAction::StaticRTTI(),
            &CreateBacoAttackAIAction, nullptr,
        };
        const bool BacoAttackRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(BacoAttackRecord);
    }

    wxBacoAttackAIAction::wxBacoAttackAIAction(
        wxBacoAttackAIActionHost& host) noexcept : host_(host) {}

    void wxBacoAttackAIAction::SetFactoryHostForAnalysis(
        wxBacoAttackAIActionHost* const host) noexcept
    {
        factoryHost = host;
    }

    const spRTTIRecord& wxBacoAttackAIAction::StaticRTTI() noexcept
    {
        (void)BacoAttackRegistered;
        return BacoAttackRecord;
    }

    void wxBacoAttackAIAction::vfunc_0C(const void*) noexcept
    {
        // PC 5B7A00 / PS2 244790: empty notification callback.
    }

    std::unique_ptr<spBaseObject> wxBacoAttackAIAction::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBacoAttackAIAction>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxBacoAttackAIAction::vfunc_14(
        spBaseObject& destination, spCloneManager& manager) const
    {
        if (!destination.IsExactly(ClassID)) return false;
        if (!wxAIAction::vfunc_14(destination, manager)) return false;
        auto& baco = static_cast<wxBacoAttackAIAction&>(destination);
        // Original Copy transfers exactly five words and one byte, after
        // clearing the destination's base-owned action graph.
        baco.target_ = target_;
        baco.state_ = state_;
        baco.field3B0_ = field3B0_;
        baco.field3B4_ = field3B4_;
        baco.field3B8_ = field3B8_;
        baco.flag3BC_ = flag3BC_;
        return true;
    }

    const spRTTIRecord& wxBacoAttackAIAction::vfunc_18() const noexcept
    {
        return BacoAttackRecord;
    }

    bool wxBacoAttackAIAction::vfunc_24() noexcept
    {
        // PC 5B88A0 / PS2 2448A0. Each in-range branch enters a distinct
        // active body. The host owns those bodies until their game dependencies
        // are measured. Both originals reset an out-of-range state to zero.
        if (state_ <= 4) host_.DispatchStateBodyForAnalysis(*this, state_);
        else state_ = 0;
        return true;
    }

    void wxBacoAttackAIAction::vfunc_30() noexcept
    {
        // PC 5B8190 / PS2 2447E0. Original registry lookup must be supplied
        // by the host; a missing target requests key 1 after all field writes.
        host_.SetCommandByteForAnalysis(0x1D, 1);
        target_ = host_.FindRegistryTargetForAnalysis();
        state_ = 4;
        field3B4_ = 0x47AFC800; // 90000.0f bits
        flag3BC_ = 0;
        field3B0_ = 0;
        SetPathIndexForAnalysis(0);
        if (target_ == nullptr) host_.SelectActionForAnalysis(1, 0);
    }

    void wxBacoAttackAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        host_.SetCommandByteForAnalysis(0x1D, 0);
        host_.SetCommandByteForAnalysis(0x20, 0);
        target_ = nullptr;
    }

    void wxBacoAttackAIAction::vfunc_38() noexcept
    {
        if (host_.FindNearestTargetForAnalysis() == nullptr)
            host_.SelectActionForAnalysis(1, 0);
    }

    void wxBacoAttackAIAction::SetOwnFieldsForAnalysis(void* const target,
        const std::uint32_t state, const std::uint32_t field3B0,
        const std::uint32_t field3B4, const std::uint32_t field3B8,
        const std::uint8_t flag3BC) noexcept
    {
        target_ = target;
        state_ = state;
        field3B0_ = field3B0;
        field3B4_ = field3B4;
        field3B8_ = field3B8;
        flag3BC_ = flag3BC;
    }
}
