#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::ps2
{
    struct wxAlfeaDoorTriggerLayout final
    {
        std::uint8_t pivotingDoor[0x1F0];
        std::uint32_t doorGroup;
        std::uint32_t doorId; // untouched by constructor
        std::uint8_t locked, queued, instant, padding[5];
    };
    static_assert(sizeof(wxAlfeaDoorTriggerLayout) == 0x200);
    static_assert(offsetof(wxAlfeaDoorTriggerLayout, instant) == 0x1FA);
    inline constexpr std::uint32_t wxAlfeaDoorTriggerConstructor = 0x003990F0;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerFactory = 0x003EB690;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerVtable = 0x00497350;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerProperties = 0x003989A0;
}
