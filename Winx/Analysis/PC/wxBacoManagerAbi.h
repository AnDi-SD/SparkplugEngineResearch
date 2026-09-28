#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxBacoManagerLayout final
    {
        std::uint8_t entityPrefix[0x124];
        std::uint32_t unknown124;
        std::uint32_t membersBegin, membersEnd, membersCapacity; // 128..130
        float sourcePositions[18];                            // 134..17b
        float transform[18];                                  // 17c..1c3
        std::uint32_t deadlines[6];                           // 1c4..1db
        std::uint32_t nextProximityCheck;                     // 1dc
        std::uint8_t reachedFirst, reachedSecond;             // 1e0/1e1
        std::uint8_t waveFirst, waveSecond;                   // 1e2/1e3
        std::uint32_t firstCount, secondCount;                // 1e4/1e8
    };
    static_assert(sizeof(wxBacoManagerLayout) == 0x1EC);
    static_assert(offsetof(wxBacoManagerLayout, membersBegin) == 0x128);
    static_assert(offsetof(wxBacoManagerLayout, deadlines) == 0x1C4);
    static_assert(offsetof(wxBacoManagerLayout, reachedFirst) == 0x1E0);
    inline constexpr std::uint32_t wxBacoManagerClassID = 0x352C4347;
    inline constexpr std::uint32_t wxBacoManagerFactory = 0x00407340;
    inline constexpr std::uint32_t wxBacoManagerVTable = 0x007005B0;
    inline constexpr std::uint32_t wxBacoManagerNotify = 0x00573950;
    inline constexpr std::uint32_t wxBacoManagerSetup = 0x00572D60;
    inline constexpr std::uint32_t wxBacoManagerUpdate = 0x00573870;
    inline constexpr std::uint32_t wxBacoManagerCheckProximity = 0x00572E80;
}
