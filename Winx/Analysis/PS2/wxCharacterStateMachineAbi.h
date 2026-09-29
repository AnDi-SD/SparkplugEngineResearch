#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxCharacterStateMachineLayout final
    {
        std::uint8_t entityPrefix[0x130];
        std::uint32_t owner;
        std::uint32_t kind;
        std::uint32_t control;
        std::uint32_t current;
        std::uint32_t previous;
        std::uint32_t gate;
        std::uint32_t previousData;
        std::uint32_t currentData;
        std::uint32_t nextData;
        std::uint32_t statePointers[42];
        std::uint32_t savedStates[5];
        std::uint32_t savedData[5];
        std::uint32_t depth;
        std::uint32_t mode;
        std::uint8_t lastFlag;
        std::uint8_t unknownTail[3];
    };
    static_assert(sizeof(wxCharacterStateMachineLayout) == 0x230);
    static_assert(offsetof(wxCharacterStateMachineLayout, current) == 0x13C);
    static_assert(offsetof(wxCharacterStateMachineLayout, statePointers) == 0x154);
    static_assert(offsetof(wxCharacterStateMachineLayout, savedStates) == 0x1FC);
    static_assert(offsetof(wxCharacterStateMachineLayout, depth) == 0x224);
}
