#include "wxBacoStateMachine.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord machineRecord{0xD32F3AA1, 0x796A1869,
            "wxCharacterStateMachine", &wxEntityRecord, nullptr, nullptr};
        std::unique_ptr<spBaseObject> CreateBacoStateMachine()
        {
            return std::make_unique<wxBacoStateMachine>();
        }
        const spRTTIRecord record{wxBacoStateMachine::ClassID, 0xD32F3AA1,
            "wxBacoStateMachine", &machineRecord, &CreateBacoStateMachine, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    const spRTTIRecord& wxBacoStateMachine::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxBacoStateMachine::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> wxBacoStateMachine::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBacoStateMachine>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxBacoStateMachine::vfunc_14(spBaseObject& destination,
        spCloneManager&) const
    {
        auto* other = dynamic_cast<wxBacoStateMachine*>(&destination);
        if (!other || setup_ || sourceFlags_ || computedFlags_) return false;
        CopyNameToForAnalysis(*other);
        return true;
    }

    std::uint32_t wxBacoStateMachine::ClassifyRequestForAnalysis(
        const std::uint32_t packedKey, const bool ownerHasActiveEntity) noexcept
    {
        const auto category = (packedKey >> 15) & 15u;
        if (category == 3) return 11;
        if (!ownerHasActiveEntity) return 11;
        if (category == 1) return 10;
        const auto action = (packedKey >> 7) & 255u;
        return action == 8 || action == 9 ? 3u : 0u;
    }

    std::uint32_t wxBacoStateMachine::ComputeFlagsForAnalysis(
        const std::uint32_t sourceFlags,
        const ControlFlagsForAnalysis controls) noexcept
    {
        auto result = sourceFlags & ~15u;
        if (controls.flag20) result = (result & 0xFFFF807Fu) | 0x400u;
        else if (controls.flag21) result = (result & 0xFFFF807Fu) | 0x480u;
        else result &= 0xFFFF807Fu;

        if (controls.flag5F) result = (result & 0xFFF87FFFu) | 0x18000u;
        else if (controls.flag5C) result = (result & 0xFFF87FFFu) | 0x8000u;
        else result &= 0xFFF87FFFu;
        return result;
    }

    void wxBacoStateMachine::UpdateFlagsForAnalysis(
        const std::uint32_t sourceFlags,
        const ControlFlagsForAnalysis controls) noexcept
    {
        sourceFlags_ = sourceFlags;
        computedFlags_ = ComputeFlagsForAnalysis(sourceFlags, controls);
    }

    void wxBacoStateMachine::SetupForAnalysis(wxBacoStateMachineHost& host)
    {
        if (setup_) throw std::logic_error("Baco machine repeat setup is unverified");
        host.PrepareMachineForAnalysis(*this);
        for (std::size_t slot = 0; slot < StateClassIDs.size(); ++slot)
        {
            auto state = host.CreateStateForAnalysis(StateClassIDs[slot]);
            if (!state) throw std::logic_error("Baco state factory returned null");
            host.BindStateForAnalysis(*state, *this);
            states_[slot] = std::move(state);
        }
        setup_ = true;
    }

    bool wxBacoStateMachine::HandleMessageForAnalysis(
        const std::uint32_t code, wxBacoStateMachineHost& host)
    {
        if (code == 0x1C)
        {
            SetupForAnalysis(host);
            return true;
        }
        if (code == 0x2717)
        {
            if (!states_[2]) throw std::logic_error("Baco hurt state is not initialized");
            host.Handle2717ForAnalysis(*states_[2]);
            return true;
        }
        return false; // inherited wxCharacterStateMachine handler is pending
    }

    wxCharacterState* wxBacoStateMachine::GetStateForAnalysis(
        const std::size_t slot) const noexcept
    {
        return slot < states_.size() ? states_[slot].get() : nullptr;
    }
}
