#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxBacoManagerLayout final
    {
        std::uint8_t entityPrefix[0x130];
        std::uint8_t unknown130[0x9C]; // constructor initializes parts of this area
        std::uint32_t unknown1CC[6];
        std::uint32_t unknown1E4;
        std::uint8_t unknown1E8[4];
        std::uint32_t unknown1EC, unknown1F0;
        std::uint8_t tail[0x0C];
    };
    static_assert(sizeof(wxBacoManagerLayout) == 0x200);
    static_assert(offsetof(wxBacoManagerLayout, unknown1CC) == 0x1CC);
    static_assert(offsetof(wxBacoManagerLayout, unknown1E8) == 0x1E8);
    inline constexpr std::uint32_t wxBacoManagerClassID = 0x352C4347;
    inline constexpr std::uint32_t wxBacoManagerFactory = 0x003E7E70;
    inline constexpr std::uint32_t wxBacoManagerConstructor = 0x002FE270;
    inline constexpr std::uint32_t wxBacoManagerVTable = 0x00495BB0;
    inline constexpr std::uint32_t wxBacoManagerRTTIGetter = 0x003F88D0;
}
