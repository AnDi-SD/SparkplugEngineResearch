#include "wxAIAction.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateAIAction()
        {
            return std::make_unique<wxAIAction>();
        }

        const spRTTIRecord AIActionRecord{
            wxAIAction::ClassID,
            spBaseObject::ClassID,
            "wxAIAction",
            &spBaseObject::StaticRTTI(),
            &CreateAIAction,
            nullptr,
        };

        const bool AIActionRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(AIActionRecord);
    }

    wxAIAction::wxAIAction() noexcept = default;

    wxAIAction::~wxAIAction()
    {
        ClearDestinationForAnalysis();
    }

    const spRTTIRecord& wxAIAction::StaticRTTI() noexcept
    {
        (void)AIActionRegistered;
        return AIActionRecord;
    }

    void wxAIAction::vfunc_0C(const void* const notification) noexcept
    {
        // PC 58EA60 / PS2 223400. The native code dereferences the message
        // unconditionally and deliberately re-reads +10 after virtual slot 2C.
        const auto* const message =
            static_cast<const wxAIActionMessageForAnalysis*>(notification);
        if (message->code == 0x1C)
        {
            (void)vfunc_2C();
        }
        if (currentAction_ != nullptr)
        {
            currentAction_->vfunc_0C(notification);
        }
    }

    std::unique_ptr<spBaseObject> wxAIAction::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxAIAction>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    bool wxAIAction::vfunc_14(
        spBaseObject& destination, spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID))
        {
            return false;
        }
        auto& actionDestination = static_cast<wxAIAction&>(destination);
        // Original 58FDD0 clears the destination first. No common payload
        // fields are transferred from the source.
        actionDestination.ClearDestinationForAnalysis();
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& wxAIAction::vfunc_18() const noexcept
    {
        return AIActionRecord;
    }

    bool wxAIAction::vfunc_24() noexcept { return true; }
    bool wxAIAction::vfunc_28() noexcept { return true; }
    bool wxAIAction::vfunc_2C() noexcept { return true; }
    void wxAIAction::vfunc_30() noexcept {}

    void wxAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        clearableWord_ = 0;
    }

    void wxAIAction::vfunc_38() noexcept {}
    bool wxAIAction::vfunc_3C(const void*) noexcept { return true; }
    bool wxAIAction::vfunc_40(const void*) noexcept { return false; }

    void wxAIAction::SetCurrentActionForAnalysis(wxAIAction* const current) noexcept
    {
        currentAction_ = current;
    }

    void wxAIAction::AddOwnedActionForAnalysis(std::unique_ptr<spBaseObject> action)
    {
        ownedActions_.push_back(std::move(action));
    }

    void wxAIAction::SetOwnerForAnalysis(void* const owner) noexcept { owner_ = owner; }
    void wxAIAction::SetField24ForAnalysis(void* const value) noexcept { field24_ = value; }
    void wxAIAction::SetClearableWordForAnalysis(const std::uint32_t value) noexcept
    {
        clearableWord_ = value;
    }

    wxAIAction* wxAIAction::GetCurrentActionForAnalysis() const noexcept
    {
        return currentAction_;
    }

    std::size_t wxAIAction::GetOwnedActionCountForAnalysis() const noexcept
    {
        return ownedActions_.size();
    }

    void* wxAIAction::GetOwnerForAnalysis() const noexcept { return owner_; }
    void* wxAIAction::GetField24ForAnalysis() const noexcept { return field24_; }

    const std::array<std::uint32_t, 7>&
        wxAIAction::GetZeroWordsForAnalysis() const noexcept
    {
        return zeroWords_;
    }

    std::uint32_t wxAIAction::GetDurationMillisecondsForAnalysis() const noexcept
    {
        return durationMilliseconds_;
    }

    std::uint32_t wxAIAction::GetClearableWordForAnalysis() const noexcept
    {
        return clearableWord_;
    }

    const std::array<float, 3>& wxAIAction::GetVector0ForAnalysis() const noexcept
    {
        return vector0_;
    }

    const std::array<float, 3>& wxAIAction::GetVector1ForAnalysis() const noexcept
    {
        return vector1_;
    }

    float wxAIAction::GetScalarBetweenVectorsForAnalysis() const noexcept
    {
        return scalarBetweenVectors_;
    }

    const std::array<float, 3>& wxAIAction::GetVector2ForAnalysis() const noexcept
    {
        return vector2_;
    }

    void wxAIAction::ClearDestinationForAnalysis() noexcept
    {
        // Native 58FD30 calls the current object's slot +2C only when its own
        // current pointer is non-null, then drops the borrowed current link.
        if (currentAction_ != nullptr
            && currentAction_->GetCurrentActionForAnalysis() != nullptr)
        {
            currentAction_->vfunc_34_ClearForAnalysis();
        }
        currentAction_ = nullptr;
        ownedActions_.clear();
    }
}
