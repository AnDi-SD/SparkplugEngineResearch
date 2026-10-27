#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
namespace winx::evidence::ps2
{
    struct wxHurtAIActionLayout final { wxAIActionLayout base;std::uint8_t latch,padding[3]; };
    static_assert(sizeof(wxHurtAIActionLayout)==0x3B0);
    static_assert(offsetof(wxHurtAIActionLayout,latch)==0x3AC);
    inline constexpr std::uint32_t wxHurtAIActionFactory=0x250C10;
    inline constexpr std::uint32_t wxHurtAIActionVtable=0x492750;
    inline constexpr std::uint32_t wxHurtAIActionUpdate=0x250A30;
    inline constexpr std::uint32_t wxHurtAIActionEntry=0x2509D0;
    inline constexpr std::uint32_t wxHurtAIActionExit=0x2509B0;
    inline constexpr std::uint32_t wxHurtAIActionCopy=0x250920;
    inline constexpr std::uint32_t wxHurtAIActionRange=0x250810;
}
