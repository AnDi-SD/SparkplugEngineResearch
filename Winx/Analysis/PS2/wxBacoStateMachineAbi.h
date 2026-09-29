#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxBacoStateMachineLayout final
    {
        std::uint8_t entityPrefix[0x130];
        std::uint32_t owner;                 // 130
        std::uint32_t kind;                  // 134: 17
        std::uint32_t control;               // 138
        std::uint8_t baseFields[0x10];       // 13c..14b
        std::uint32_t sourceFlags;           // 14c
        std::uint32_t computedFlags;         // 150
        std::uint32_t movingState;           // 154
        std::uint8_t baseStateSlots[8];      // 158..15f
        std::uint32_t attackingState;        // 160
        std::uint8_t intermediateSlots[0x18];// 164..17b
        std::uint32_t hurtState;             // 17c
        std::uint32_t dyingState;            // 180
        std::uint8_t tail[0xAC];             // 184..22f
    };
    static_assert(sizeof(wxBacoStateMachineLayout) == 0x230);
    static_assert(offsetof(wxBacoStateMachineLayout, kind) == 0x134);
    static_assert(offsetof(wxBacoStateMachineLayout, sourceFlags) == 0x14C);
    static_assert(offsetof(wxBacoStateMachineLayout, hurtState) == 0x17C);
    inline constexpr std::uint32_t wxBacoStateMachineClassID = 0x78603414;
    inline constexpr std::uint32_t wxBacoStateMachineFactory = 0x003EEAA0;
    inline constexpr std::uint32_t wxBacoStateMachineConstructor = 0x002FF000;
    inline constexpr std::uint32_t wxBacoStateMachineVTable = 0x00498890;
    inline constexpr std::uint32_t wxBacoStateMachineSetup = 0x002FEDB0;
    inline constexpr std::uint32_t wxBacoStateMachineComputeFlags = 0x002FEC30;
    inline constexpr std::uint32_t wxBacoStateMachineClassify = 0x002FEBB0;
    inline constexpr std::uint32_t wxBacoStateMachineNotify = 0x002FED40;
}
