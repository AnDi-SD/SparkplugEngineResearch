#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t GiantBoulderTriggerClassId = 0x55E957F4;
    inline constexpr std::uint32_t GiantBoulderTriggerPrimary = 0x006FD000;
    inline constexpr std::uint32_t GiantBoulderTriggerProximity = 0x00545E50;
    inline constexpr std::size_t GiantBoulderTriggerProximitySlot = 0x4C;
    inline constexpr std::size_t GiantBoulderTriggerFirstPoint = 0x148;
    inline constexpr std::size_t GiantBoulderTriggerSecondPoint = 0x14C;
    inline constexpr std::size_t GiantBoulderTriggerPointX = 0x74;
    inline constexpr std::size_t GiantBoulderTriggerPointY = 0x78;
    inline constexpr std::size_t GiantBoulderTriggerPointZ = 0x7C;
    inline constexpr std::uint32_t GiantBoulderTriggerReadPoints = 0x00597660;
    inline constexpr std::uint32_t GiantBoulderTriggerSquaredDistance = 0x00597600;
    inline constexpr std::uint32_t GiantBoulderTriggerLimitAddress = 0x006FCFF8;
    inline constexpr float GiantBoulderTriggerSquaredLimit = 160000.0F;
} // namespace winx::evidence::pc
