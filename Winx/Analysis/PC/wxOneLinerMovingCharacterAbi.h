#pragma once

#include <cstddef>
#include <cstdint>

namespace wxOneLinerMovingCharacterAbi
{
inline constexpr std::uint32_t CapturedClassId = 0x415414B0;
inline constexpr std::uint32_t CapturedPrimary = 0x006FE124;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 296;
inline constexpr std::size_t OneLinerIdRawDwordOffset = 0x124;
inline constexpr std::uint32_t OneLinerIdSetterPC = 0x00555040;
inline constexpr std::uint32_t OneLinerIdGetterPC = 0x00555050;
}
