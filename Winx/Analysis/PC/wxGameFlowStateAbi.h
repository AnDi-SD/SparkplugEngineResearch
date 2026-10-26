#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxGameFlowStateLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t field10;
        std::uint8_t field14, padding15[3];
        std::uint32_t codes[3];
        std::uint8_t field24, field25, padding26[2];
        std::uint32_t unwritten28, ownedTimer2C, borrowedInput30, field34;
        std::uint8_t field38, padding39[3];
    };
    static_assert(sizeof(wxGameFlowStateLayout) == 0x3C);
    static_assert(offsetof(wxGameFlowStateLayout, codes) == 0x18);
    static_assert(offsetof(wxGameFlowStateLayout, ownedTimer2C) == 0x2C);
    inline constexpr std::uint32_t wxGameFlowStateFactory = 0x005D6DC0;
    inline constexpr std::uint32_t wxGameFlowStateConstructor = 0x005D6B20;
    inline constexpr std::uint32_t wxGameFlowStateVTable = 0x0070F188;
    inline constexpr std::uint32_t wxGameFlowStateSetCodes = 0x005D6850;
    inline constexpr std::uint32_t wxGameFlowStateResetResources = 0x005D6880;
    inline constexpr std::uint32_t wxGameFlowStateRemapResource = 0x005D6CB0;
}
