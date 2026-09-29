#pragma once

#include "wxCharacterStateMachine.h"
#include "Code/wxCharacterState.h"
#include "Analysis/Host/wxBacoStateMachineHost.h"

#include <array>
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    // The native parent and the portable parent are the same common machine.
    // Scene setup still uses a host until the full binding is known.
    class wxBacoStateMachine final : public wxCharacterStateMachine
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x78603414;
        static constexpr std::uint32_t MachineKind = 17;
        static constexpr std::array<std::uint32_t, 4> StateClassIDs = {
            0x463733DF, // wxSpiderMovingState
            0x6ADD2466, // wxSpiderAttackState
            0x54F716CC, // wxSpiderHurtState
            0xBCC87DA1  // wxDyingState
        };
        static constexpr std::array<std::size_t, 4> StateSlots = {0, 3, 10, 11};

        struct ControlFlagsForAnalysis final
        {
            bool flag20 = false;
            bool flag21 = false;
            bool flag5F = false;
            bool flag5C = false;
        };

        wxBacoStateMachine() noexcept = default;
        ~wxBacoStateMachine() override = default;
        wxBacoStateMachine(const wxBacoStateMachine&) = delete;
        wxBacoStateMachine& operator=(const wxBacoStateMachine&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        // Native slot 14 returns false. No base-machine transition is implied.
        [[nodiscard]] bool RejectTransitionForAnalysis() const noexcept { return false; }

        // Slot 19: read the packed category first, then the owner presence.
        [[nodiscard]] static std::uint32_t ClassifyRequestForAnalysis(
            std::uint32_t packedKey, bool ownerHasActiveEntity) noexcept;

        // Slot 18: returns the derived bitfield. Native PC stores it at +144.
        [[nodiscard]] static std::uint32_t ComputeFlagsForAnalysis(
            std::uint32_t sourceFlags, ControlFlagsForAnalysis controls) noexcept;
        void UpdateFlagsForAnalysis(std::uint32_t sourceFlags,
            ControlFlagsForAnalysis controls) noexcept;
        [[nodiscard]] std::uint32_t GetComputedFlagsForAnalysis() const noexcept
        { return computedFlags_; }

        // Slot 17 and the two leaf-specific notification branches. The host
        // supplies the missing base machine and actual state implementations.
        void SetupForAnalysis(wxBacoStateMachineHost& host);
        [[nodiscard]] bool HandleMessageForAnalysis(std::uint32_t code,
            wxBacoStateMachineHost& host);
        [[nodiscard]] wxCharacterState* GetStateForAnalysis(std::size_t slot) const noexcept;

    private:
        std::uint32_t sourceFlags_ = 0;
        std::uint32_t computedFlags_ = 0;
        std::array<std::unique_ptr<wxCharacterState>, 4> states_{};
        bool setup_ = false;
    };
}
