#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    class wxAttackAIAction;

    // Our adapter for the owner, character, command, target registry and timer.
    // Targets are borrowed. The host must outlive its actions. There is no
    // successful default implementation for unavailable game systems.
    class wxAttackAIActionHost
    {
    public:
        virtual ~wxAttackAIActionHost() = default;
        virtual void SetCommandByteForAnalysis(std::uint32_t offset,
            std::uint8_t value) noexcept = 0;
        virtual bool GetOwnerFlag1C5ForAnalysis() noexcept = 0;
        virtual void* FindRegistryTargetForAnalysis() noexcept = 0;
        virtual void* FindNearestTargetForAnalysis() noexcept = 0;
        virtual std::uint32_t GetTimerBaseForAnalysis() noexcept = 0;
        virtual std::uint32_t GetOwnerMinForAnalysis() noexcept = 0;
        virtual std::uint32_t GetOwnerMaxForAnalysis() noexcept = 0;
        virtual std::uint32_t NextRandomForAnalysis() noexcept = 0;
        virtual void SelectActionForAnalysis(std::uint32_t key,
            std::uint32_t parameter) noexcept = 0;
        // Original state handlers occupy slots 17..24, after the known base
        // vtable. Their active bodies remain outside this reconstruction.
        virtual void DispatchSlotForAnalysis(wxAttackAIAction&,
            std::uint32_t sourceSlot, const void* argument) noexcept = 0;
    };
}
