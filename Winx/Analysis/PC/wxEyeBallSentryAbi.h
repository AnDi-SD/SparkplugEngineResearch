#pragma once
#include <cstddef>
#include <cstdint>

namespace wxEyeBallSentryAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00700DE8;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x148;
inline constexpr std::size_t DamageRawOffset = 0x144;
inline constexpr std::uint32_t DamageSetterPC = 0x0056B080;
inline constexpr std::uint32_t DamageGetterPC = 0x0057FC30;
// Historical allocation416B is separate from the selected328B minimum prefix.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
