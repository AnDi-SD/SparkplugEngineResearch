#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
namespace winx::evidence::pc
{
    struct wxHurtAIActionLayout final { wxAIActionLayout base;std::uint8_t latch,padding[3]; };
    static_assert(sizeof(wxHurtAIActionLayout)==0x3AC);
    static_assert(offsetof(wxHurtAIActionLayout,latch)==0x3A8);
    inline constexpr std::uint32_t wxHurtAIActionFactory=0x5A9F50;
    inline constexpr std::uint32_t wxHurtAIActionVtable=0x708DE0;
    inline constexpr std::uint32_t wxHurtAIActionRTTIGetter=0x5A9E40;
    inline constexpr std::uint32_t wxHurtAIActionUpdate=0x5A9E60;
    inline constexpr std::uint32_t wxHurtAIActionEntry=0x5A9ED0;
    inline constexpr std::uint32_t wxHurtAIActionExit=0x5A9F30;
    inline constexpr std::uint32_t wxHurtAIActionCopy=0x5AA030;
    //18th vtable slot+44;17-entry historical catalogs omit this own method.
    inline constexpr std::uint32_t wxHurtAIActionRange=0x5AA070;
}
