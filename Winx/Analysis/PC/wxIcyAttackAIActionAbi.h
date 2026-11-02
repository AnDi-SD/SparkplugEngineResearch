#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    // The direct same-receiver58FF00 constructor prefix establishes the
    // AIAction prefix. This layout is separate from the host C++ facade.
    struct wxIcyAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[0xD4];
    };
    static_assert(sizeof(wxIcyAttackAIActionLayout) == 0x480);
    static_assert(offsetof(wxIcyAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxIcyAttackAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxIcyAttackAIActionClassID = 0x510661EC;
    inline constexpr Address32 wxIcyAttackAIActionHistoricalFactory = 0x5C1C00;
    inline constexpr Address32 wxIcyAttackAIActionVTable = 0x70A0C0;
    inline constexpr Address32 wxIcyAttackAIActionRTTIGetter = 0x5C1580;
    inline constexpr Address32 wxIcyAttackAIActionRTTIRecord = 0x7679D0;
    inline constexpr Address32 wxIcyAttackAIActionEntry = 0x5BE230;
    inline constexpr Address32 wxIcyAttackAIActionExit = 0x5BE2C0;
    inline constexpr Address32 wxIcyAttackAIActionExistingProfileGlobal = 0x765AD4;
    inline constexpr std::uint32_t wxIcyAttackAIActionExitProfileByteOffset = 0x510;
    inline constexpr Address32 wxIcyAttackAIActionRegistryCall = 0x5BE24B;
    inline constexpr Address32 wxIcyAttackAIActionRegistryReturn = 0x5BE250;
}
