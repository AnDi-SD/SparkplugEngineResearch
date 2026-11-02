#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxHelpAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint8_t capturedOwnBytes[0xC0];
    };
    static_assert(sizeof(wxHelpAIActionLayout) == 0x468);
    static_assert(offsetof(wxHelpAIActionLayout, capturedOwnBytes) == 0x3A8);
    inline constexpr std::uint32_t wxHelpAIActionClassID = 0x67435487;
    inline constexpr Address32 wxHelpAIActionFactory = 0x5BB0B0;
    inline constexpr Address32 wxHelpAIActionFactoryCalledConstructor = 0x5BADB0;
    inline constexpr Address32 wxHelpAIActionVTable = 0x709C50;
    inline constexpr Address32 wxHelpAIActionRTTIGetter = 0x5BAE70;
    inline constexpr Address32 wxHelpAIActionRTTIRecord = 0x767850;
    inline constexpr Address32 wxHelpAIActionEntry = 0x5BAF70;
    inline constexpr Address32 wxHelpAIActionDeletingDestructor = 0x5BB160;
    inline constexpr Address32 wxHelpAIActionDestructor = 0x5BAE80;
}
