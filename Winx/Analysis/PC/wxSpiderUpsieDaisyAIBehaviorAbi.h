#pragma once
#include <cstddef>
#include <cstdint>

namespace wxSpiderUpsieDaisyAIBehaviorAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00700098;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x138;
inline constexpr std::size_t SpiderNumRawOffset = 0x134;
inline constexpr std::uint32_t SpiderNumSetterPC = 0x0056A5F0;
inline constexpr std::uint32_t SpiderNumGetterPC = 0x0057FC00;
// Historical allocation332B is separate from the selected312B minimum prefix.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
