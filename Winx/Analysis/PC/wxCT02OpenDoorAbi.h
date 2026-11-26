#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t CT02OpenDoorClassId = 0x28734422;
    inline constexpr std::uint32_t CT02OpenDoorPrimary = 0x006FD150;
    inline constexpr std::uint32_t CT02OpenDoorProximity = 0x00546D30;
    inline constexpr std::size_t CT02OpenDoorProximitySlot = 0x4C;
    inline constexpr std::size_t CT02OpenDoorFirstPoint = 0x14C;
    inline constexpr std::size_t CT02OpenDoorSecondPoint = 0x148;
    inline constexpr std::size_t CT02OpenDoorPointX = 0x74;
    inline constexpr std::size_t CT02OpenDoorPointY = 0x78;
    inline constexpr std::size_t CT02OpenDoorPointZ = 0x7C;
    inline constexpr std::uint32_t CT02OpenDoorReadPoints = 0x00597660;
    inline constexpr std::uint32_t CT02OpenDoorSquaredDistance = 0x00597600;
    inline constexpr std::uint32_t CT02OpenDoorLimitAddress = 0x006FD148;
    inline constexpr float CT02OpenDoorHeightTolerance = 100.0F;
    inline constexpr float CT02OpenDoorSquaredLimit = 20000.0F;
} // namespace winx::evidence::pc
