#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::pc
{
    struct wxAlfeaDoorTriggerLayout final
    {
        std::uint8_t pivotingDoor[0x1CC];
        std::uint32_t doorGroup;
        std::uint32_t doorId; // untouched by constructor
        std::uint8_t locked, queued, instant, padding; // padding untouched
    };
    static_assert(sizeof(wxAlfeaDoorTriggerLayout) == 0x1D8);
    static_assert(offsetof(wxAlfeaDoorTriggerLayout, instant) == 0x1D6);
    inline constexpr std::uint32_t wxAlfeaDoorTriggerConstructor = 0x00547E50;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerFactory = 0x00405E40;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerVtable = 0x006FD3F8;
    inline constexpr std::uint32_t wxAlfeaDoorTriggerProperties = 0x005484B0;
}
