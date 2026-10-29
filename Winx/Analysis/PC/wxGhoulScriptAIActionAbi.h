#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxGhoulScriptAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t state3A8;
        std::uint32_t word3AC;
        Address32 borrowedNode3B0;
        std::uint32_t opaque3B4;
        Address32 borrowedCommand3B8;
    };
    static_assert(sizeof(wxGhoulScriptAIActionLayout) == 0x3BC);
    static_assert(offsetof(wxGhoulScriptAIActionLayout, borrowedNode3B0) == 0x3B0);
    static_assert(offsetof(wxGhoulScriptAIActionLayout, borrowedCommand3B8) == 0x3B8);
    inline constexpr std::uint32_t wxGhoulScriptAIActionClassID = 0x75E255E9;
    inline constexpr Address32 wxGhoulScriptAIActionFactory = 0x5A9860;
    inline constexpr Address32 wxGhoulScriptAIActionFactoryCalledConstructor = 0x5A95C0;
    inline constexpr Address32 wxGhoulScriptAIActionVTable = 0x708D68;
    inline constexpr Address32 wxGhoulScriptAIActionRTTIGetter = 0x5A9680;
    inline constexpr Address32 wxGhoulScriptAIActionRTTIRecord = 0x766F50;
    inline constexpr Address32 wxGhoulScriptAIActionEntry = 0x5A9930;
    inline constexpr Address32 wxGhoulScriptAIActionDeletingDestructor = 0x5A9910;
    inline constexpr Address32 wxGhoulScriptAIActionDestructor = 0x5A9690;
}
