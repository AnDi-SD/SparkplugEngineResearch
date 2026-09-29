#pragma once

#include "Code/SparkBase/spBaseObject.h"
#include "wxCharacterState.h"
#include "Analysis/Host/wxCharacterStateMachineHost.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    // Native physical parent: wxEntity. The portable wxEntity implementation
    // is pending, so scene callbacks and 32-bit layout remain separate.
    class wxCharacterStateMachine : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0xD32F3AA1;
        static constexpr std::uint32_t MachineKind = 68;
        static constexpr std::size_t StateCount = 42;
        static constexpr std::size_t StackCapacity = 5;

        struct SnapshotForAnalysis final
        {
            std::uint32_t current = 0;
            std::uint32_t previous = 0;
            std::uint32_t gate = 1;
            std::uint32_t previousData = 0;
            std::uint32_t currentData = 0;
            std::uint32_t nextData = 0;
            std::uint32_t depth = 0;
            std::uint32_t mode = 0;
            std::uint8_t lastFlag = 1;
        };

        wxCharacterStateMachine() noexcept = default;
        ~wxCharacterStateMachine() override = default;
        wxCharacterStateMachine(const wxCharacterStateMachine&) = delete;
        wxCharacterStateMachine& operator=(const wxCharacterStateMachine&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        void SetHostForAnalysis(wxCharacterStateMachineHost* host) noexcept { host_ = host; }
        void SetStateForAnalysis(std::size_t index, wxCharacterState* state);
        [[nodiscard]] wxCharacterState* GetStateForAnalysis(std::size_t index) const noexcept;
        void SetRuntimeForAnalysis(SnapshotForAnalysis value) noexcept { runtime_ = value; }
        [[nodiscard]] SnapshotForAnalysis GetRuntimeForAnalysis() const noexcept { return runtime_; }
        void SetSavedForAnalysis(std::size_t index, std::uint32_t state,
            std::uint32_t data);
        [[nodiscard]] std::uint32_t GetSavedStateForAnalysis(std::size_t index) const;
        [[nodiscard]] std::uint32_t GetSavedDataForAnalysis(std::size_t index) const;

        // Base machine slot 19 selects index zero. Derived machines replace it.
        [[nodiscard]] virtual std::uint32_t SelectStateForAnalysis(
            wxAnimationRequestForAnalysis& nextData) const noexcept;
        [[nodiscard]] bool EnterCurrentForAnalysis();
        void ResetForAnalysis();
        void SwitchToNextForAnalysis();
        void PushCurrentForAnalysis();
        void PopSavedForAnalysis();
        void SetModeFromCodeForAnalysis(std::uint32_t code) noexcept;
        void HandleMessageForAnalysis(std::uint32_t code, const void* message);

        // Native slot 11 reads the bound object's flags through machine+24.
        [[nodiscard]] static bool AllowsBoundFlagsForAnalysis(std::uint32_t flags) noexcept
        { return (flags & 0x18u) == 0; }

    private:
        [[nodiscard]] wxCharacterState& RequireStateForAnalysis(std::uint32_t index) const;
        [[nodiscard]] wxCharacterStateMachineHost& RequireHostForAnalysis() const;
        [[nodiscard]] bool IsFactoryDefaultForAnalysis() const noexcept;

        SnapshotForAnalysis runtime_{};
        std::array<wxCharacterState*, StateCount> states_{}; // borrowed
        std::array<std::uint32_t, StackCapacity> savedStates_{};
        std::array<std::uint32_t, StackCapacity> savedData_{};
        wxCharacterStateMachineHost* host_ = nullptr;
    };
}
