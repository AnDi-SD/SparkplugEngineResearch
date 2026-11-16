#pragma once
#include <cstddef>
#include <cstdint>

namespace wxCabinetPuzzleAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00700EA4;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x174;
inline constexpr std::size_t Digit4RawOffset = 0x170;
inline constexpr std::size_t Digit3RawOffset = 0x15C;
inline constexpr std::size_t Digit2RawOffset = 0x148;
inline constexpr std::size_t Digit1RawOffset = 0x134;
inline constexpr std::uint32_t Digit4SetterPC = 0x005427C0;
inline constexpr std::uint32_t Digit4GetterPC = 0x00556E00;
inline constexpr std::uint32_t Digit3SetterPC = 0x00567EA0;
inline constexpr std::uint32_t Digit3GetterPC = 0x00540D40;
inline constexpr std::uint32_t Digit2SetterPC = 0x00540CA0;
inline constexpr std::uint32_t Digit2GetterPC = 0x00540CB0;
inline constexpr std::uint32_t Digit1SetterPC = 0x0056A5F0;
inline constexpr std::uint32_t Digit1GetterPC = 0x0057FC00;
// Historical factory allocation468B is separate from the touched372B prefix.
// Current constructor body, physical inheritance and native sizeof are unknown.
}
