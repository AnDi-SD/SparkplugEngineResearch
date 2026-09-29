#include "wxCharacterStateMachine.h"

#include <algorithm>
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
        std::unique_ptr<spBaseObject> CreateCharacterStateMachine()
        {
            return std::make_unique<wxCharacterStateMachine>();
        }
        const spRTTIRecord record{wxCharacterStateMachine::ClassID, 0x796A1869,
            "wxCharacterStateMachine", &wxEntityRecord,
            &CreateCharacterStateMachine, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    const spRTTIRecord& wxCharacterStateMachine::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxCharacterStateMachine::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> wxCharacterStateMachine::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxCharacterStateMachine>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxCharacterStateMachine::vfunc_14(spBaseObject& destination,
        spCloneManager&) const
    {
        auto* other = dynamic_cast<wxCharacterStateMachine*>(&destination);
        if (!other || !IsFactoryDefaultForAnalysis()) return false;
        CopyNameToForAnalysis(*other);
        return true;
    }

    bool wxCharacterStateMachine::IsFactoryDefaultForAnalysis() const noexcept
    {
        const SnapshotForAnalysis defaults{};
        return host_ == nullptr
            && runtime_.current == defaults.current
            && runtime_.previous == defaults.previous
            && runtime_.gate == defaults.gate
            && runtime_.previousData == defaults.previousData
            && runtime_.currentData == defaults.currentData
            && runtime_.nextData == defaults.nextData
            && runtime_.depth == defaults.depth
            && runtime_.mode == defaults.mode
            && runtime_.lastFlag == defaults.lastFlag
            && std::all_of(states_.begin(), states_.end(),
                [](wxCharacterState* state) { return state == nullptr; })
            && std::all_of(savedStates_.begin(), savedStates_.end(),
                [](std::uint32_t value) { return value == 0; })
            && std::all_of(savedData_.begin(), savedData_.end(),
                [](std::uint32_t value) { return value == 0; });
    }

    void wxCharacterStateMachine::SetStateForAnalysis(
        const std::size_t index, wxCharacterState* state)
    {
        if (index >= StateCount) throw std::out_of_range("machine state index");
        states_[index] = state;
    }

    wxCharacterState* wxCharacterStateMachine::GetStateForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < StateCount ? states_[index] : nullptr;
    }

    void wxCharacterStateMachine::SetSavedForAnalysis(const std::size_t index,
        const std::uint32_t state, const std::uint32_t data)
    {
        if (index >= StackCapacity) throw std::out_of_range("machine stack index");
        savedStates_[index] = state;
        savedData_[index] = data;
    }

    std::uint32_t wxCharacterStateMachine::GetSavedStateForAnalysis(
        const std::size_t index) const
    {
        return savedStates_.at(index);
    }

    std::uint32_t wxCharacterStateMachine::GetSavedDataForAnalysis(
        const std::size_t index) const
    {
        return savedData_.at(index);
    }

    wxCharacterState& wxCharacterStateMachine::RequireStateForAnalysis(
        const std::uint32_t index) const
    {
        if (index >= StateCount || !states_[index])
            throw std::logic_error("wxCharacterStateMachine state is not bound");
        return *states_[index];
    }

    wxCharacterStateMachineHost& wxCharacterStateMachine::RequireHostForAnalysis() const
    {
        if (!host_) throw std::logic_error("wxCharacterStateMachine requires a host");
        return *host_;
    }

    std::uint32_t wxCharacterStateMachine::SelectStateForAnalysis(
        wxAnimationRequestForAnalysis&) const noexcept
    {
        return 0;
    }

    bool wxCharacterStateMachine::EnterCurrentForAnalysis()
    {
        auto request = wxAnimationRequestForAnalysis{runtime_.currentData};
        const bool accepted = RequireStateForAnalysis(runtime_.current).vfunc_1C(request);
        runtime_.currentData = request.packedKey;
        if (accepted) runtime_.gate = 0;
        return accepted;
    }

    void wxCharacterStateMachine::ResetForAnalysis()
    {
        wxCharacterState& state = RequireStateForAnalysis(0);
        runtime_.current = 0;
        runtime_.previous = 0;
        runtime_.gate = 1;
        runtime_.previousData = 0;
        runtime_.currentData = 0;
        runtime_.nextData = 0;
        runtime_.depth = 0;
        runtime_.mode = 0;
        auto request = wxAnimationRequestForAnalysis{runtime_.currentData};
        (void)state.vfunc_1C(request); // Direct call does not clear gate.
        runtime_.currentData = request.packedKey;
        runtime_.lastFlag = 1;
    }

    void wxCharacterStateMachine::SwitchToNextForAnalysis()
    {
        wxCharacterStateMachineHost& host = RequireHostForAnalysis();
        runtime_.previous = runtime_.current;
        runtime_.previousData = runtime_.currentData;
        auto request = wxAnimationRequestForAnalysis{runtime_.nextData};
        runtime_.current = SelectStateForAnalysis(request);
        runtime_.currentData = request.packedKey;
        host.DispatchEventForAnalysis(*this, 0x2720,
            runtime_.current, runtime_.currentData);
        runtime_.gate = 1;
        wxCharacterState& state = RequireStateForAnalysis(runtime_.current);
        const auto flags = state.GetTransitionFlagsForAnalysis();
        state.SetTransitionFlagsForAnalysis(true, true, true, flags[3], flags[4]);
        (void)EnterCurrentForAnalysis();
    }

    void wxCharacterStateMachine::PushCurrentForAnalysis()
    {
        wxCharacterStateMachineHost& host = RequireHostForAnalysis();
        if (runtime_.depth >= StackCapacity)
            throw std::logic_error("native caller stack precondition not established");
        wxCharacterState& state = RequireStateForAnalysis(runtime_.current);
        savedStates_[runtime_.depth] = runtime_.current;
        savedData_[runtime_.depth] = runtime_.currentData;
        ++runtime_.depth;
        host.DispatchEventForAnalysis(*this, 0x2722,
            runtime_.current, runtime_.currentData);
        auto request = wxAnimationRequestForAnalysis{runtime_.currentData};
        state.vfunc_28(request);
        runtime_.currentData = request.packedKey;
        SwitchToNextForAnalysis();
    }

    void wxCharacterStateMachine::PopSavedForAnalysis()
    {
        wxCharacterStateMachineHost& host = RequireHostForAnalysis();
        if (runtime_.depth == 0 || runtime_.depth > StackCapacity)
            throw std::logic_error("native caller stack precondition not established");
        runtime_.previous = runtime_.current;
        runtime_.previousData = runtime_.currentData;
        --runtime_.depth;
        runtime_.current = savedStates_[runtime_.depth];
        runtime_.currentData = savedData_[runtime_.depth];
        runtime_.gate = 0;
        host.DispatchEventForAnalysis(*this, 0x2723,
            runtime_.current, runtime_.currentData);
        wxCharacterState& state = RequireStateForAnalysis(runtime_.current);
        auto request = wxAnimationRequestForAnalysis{runtime_.currentData};
        state.vfunc_2C(request);
        runtime_.currentData = request.packedKey;
    }

    void wxCharacterStateMachine::SetModeFromCodeForAnalysis(
        const std::uint32_t code) noexcept
    {
        if (code == 0) runtime_.mode = 0;
        else if (code == 1) runtime_.mode = 5;
        else if (code == 3) runtime_.mode = 6;
    }

    void wxCharacterStateMachine::HandleMessageForAnalysis(
        const std::uint32_t code, const void* message)
    {
        if (code == 0x1C) RequireHostForAnalysis().HandleCode28ForAnalysis(*this);
        else if (code == 0x1E) RequireHostForAnalysis().HandleCode30ForAnalysis(*this);
        else if (code == 0x27F3) ResetForAnalysis();
        else if (auto* state = GetStateForAnalysis(runtime_.current))
            state->vfunc_0C(message);
    }
}
