#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxBacoStateMachineLayout final
    {
        std::uint8_t entityPrefix[0x124];
        std::uint32_t owner;                 // 124
        std::uint32_t kind;                  // 128: 17
        std::uint32_t control;               // 12c
        std::uint8_t baseFields[0x10];       // 130..13f
        std::uint32_t sourceFlags;           // 140
        std::uint32_t computedFlags;         // 144
        std::uint32_t movingState;           // 148
        std::uint8_t baseStateSlots[8];      // 14c..153
        std::uint32_t attackingState;        // 154
        std::uint8_t intermediateSlots[0x18];// 158..16f
        std::uint32_t hurtState;             // 170
        std::uint32_t dyingState;            // 174
        std::uint8_t tail[0xAC];             // 178..223
    };
    static_assert(sizeof(wxBacoStateMachineLayout) == 0x224);
    static_assert(offsetof(wxBacoStateMachineLayout, kind) == 0x128);
    static_assert(offsetof(wxBacoStateMachineLayout, sourceFlags) == 0x140);
    static_assert(offsetof(wxBacoStateMachineLayout, hurtState) == 0x170);
    inline constexpr std::uint32_t wxBacoStateMachineClassID = 0x78603414;
    inline constexpr std::uint32_t wxBacoStateMachineFactory = 0x00404AC0;
    inline constexpr std::uint32_t wxBacoStateMachineVTable = 0x006FADE0;
    inline constexpr std::uint32_t wxBacoStateMachineSetup = 0x0052D540;
    inline constexpr std::uint32_t wxBacoStateMachineComputeFlags = 0x0052D3B0;
    inline constexpr std::uint32_t wxBacoStateMachineClassify = 0x0052D450;
    inline constexpr std::uint32_t wxBacoStateMachineNotify = 0x0052D6F0;
}
