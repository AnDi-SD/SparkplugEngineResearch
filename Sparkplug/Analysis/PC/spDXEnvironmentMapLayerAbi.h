#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t DXEnvironmentMapLayerClassId = 0x3F424FCD;
    inline constexpr std::uint32_t DXEnvironmentMapLayerRegisteredBase = 0x427C7480;
    inline constexpr std::uint32_t DXEnvironmentMapLayerPrimary = 0x006F0B78;
    inline constexpr std::uint32_t DXEnvironmentMapLayerCopyEntry = 0x004C1120;
    inline constexpr std::size_t DXEnvironmentMapLayerCopySlotOffset = 0x0C;
    inline constexpr std::uint32_t DXEnvironmentMapLayerOwnCopyEntry = 0x004C1137;
    inline constexpr std::uint32_t DXEnvironmentMapLayerOwnCopyExclusiveStop = 0x004C113D;
    inline constexpr std::size_t DXEnvironmentMapLayerCaptureBytes = 0x18;
    inline constexpr std::size_t DXEnvironmentMapLayerOwnWordOffset = 0x14;
    inline constexpr std::size_t DXEnvironmentMapLayerOwnWordBytes = 4;
}
