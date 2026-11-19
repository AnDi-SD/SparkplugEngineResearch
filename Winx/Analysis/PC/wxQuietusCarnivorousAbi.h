#pragma once
#include <cstddef>
#include <cstdint>

namespace wxQuietusCarnivorousAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006FFD30;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x168;
inline constexpr std::size_t DamageRawOffset = 0x164;
inline constexpr std::uint32_t DamageSetterPC = 0x00567EB0;
inline constexpr std::uint32_t DamageGetterPC = 0x0054DE10;
// Historical allocation368B is separate from the selected360B minimum prefix.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
