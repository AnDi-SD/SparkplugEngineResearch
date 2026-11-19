#pragma once
#include <cstddef>
#include <cstdint>

namespace wxSwampGazBubbleAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006FE408;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x150;
inline constexpr std::size_t DamageInflictedRawOffset = 0x14C;
inline constexpr std::uint32_t DamageInflictedSetterPC = 0x00540CE0;
inline constexpr std::uint32_t DamageInflictedGetterPC = 0x00540CF0;
// Historical allocation352B is separate from the selected336B prefix.
// Current constructor body, native sizeof and physical inheritance remain unknown.
inline constexpr std::size_t GreenRawByteOffset = 0x144;
inline constexpr std::uint32_t GreenSetterPC = 0x0055F260;
inline constexpr std::uint32_t GreenGetterPC = 0x00558A20;
// Original SET reads one byte at argument-slot +4; GET/SET update AL only.
// The own value facade returns a byte, without declaring native return ABI.
}
