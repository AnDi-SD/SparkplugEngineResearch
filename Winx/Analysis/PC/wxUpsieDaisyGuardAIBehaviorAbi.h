#pragma once
#include <cstddef>
#include <cstdint>

namespace wxUpsieDaisyGuardAIBehaviorAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x007001E0;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x188;
inline constexpr std::size_t GuardNumRawOffset = 0x184;
inline constexpr std::uint32_t GuardNumSetterPC = 0x00541CA0;
inline constexpr std::uint32_t GuardNumGetterPC = 0x00539560;
// Historical allocation488B is separate from the selected392B minimum prefix.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
