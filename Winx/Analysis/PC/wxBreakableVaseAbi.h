#pragma once
#include <cstddef>
#include <cstdint>

namespace wxBreakableVaseAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006FC7A8;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x180;
inline constexpr std::size_t FairyStarsValueRawOffset = 0x17C;
inline constexpr std::uint32_t FairyStarsValueSetterPC = 0x00541430;
inline constexpr std::uint32_t FairyStarsValueGetterPC = 0x00541440;
// Historical allocation384B equals the selected384B minimum prefix; equality does not establish current sizeof.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
