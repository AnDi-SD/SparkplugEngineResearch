#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Identity/selected-consumer evidence. No full188B source layout or
    // secondary-base/Timer/matrix constructor is inferred from these offsets.
    inline constexpr std::size_t spCinematicManagerNativeExtent=0xBC;
    inline constexpr std::size_t spCinematicManagerListSentinelOffset=0x18;
    inline constexpr std::size_t spCinematicManagerListCountOffset=0x1C;
    inline constexpr std::uint32_t spCinematicManagerFactory=0x4533A0;
    inline constexpr std::uint32_t spCinematicManagerTable=0x6E6DF0;
    inline constexpr std::uint32_t spCinematicManagerGetter=0x453000;
    inline constexpr std::uint32_t spCinematicManagerRecord=0x75F820;
    inline constexpr std::uint32_t spCinematicManagerRemoveByName=0x453020;
    inline constexpr std::uint32_t spCinematicManagerDelete=0x453130;
    inline constexpr std::uint32_t spCinematicManagerCallerPrefix=0x4F82AC;
    inline constexpr std::uint32_t spCinematicManagerCallerContinuation=0x4F82D0;
    inline constexpr std::uint32_t spCinematicManagerSingleton=0x75DBA0;
}
