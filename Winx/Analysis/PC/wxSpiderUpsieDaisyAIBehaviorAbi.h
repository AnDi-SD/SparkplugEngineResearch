#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace wxSpiderUpsieDaisyAIBehaviorAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00700098;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x138;
inline constexpr std::size_t SpiderNumRawOffset = 0x134;
inline constexpr std::uint32_t SpiderNumSetterPC = 0x0056A5F0;
inline constexpr std::uint32_t SpiderNumGetterPC = 0x0057FC00;
inline constexpr std::uint32_t ClassId = 0x7EEA7E0B;
inline constexpr std::uint32_t OwnPublisherPC = 0x0056ADE0;
inline constexpr std::size_t SpeedMinimumPrefixBytes = 0x144;
inline constexpr std::array<std::size_t, 2> SpeedOffsets{0x13C, 0x140};
inline constexpr std::array<std::uint32_t, 2> SpeedSettersPC{0x004E2D60, 0x0056A600};
inline constexpr std::array<std::uint32_t, 2> SpeedGettersPC{0x00510110, 0x004F2B00};
// Historical allocation 332 B is separate from the 312 B and 324 B selected prefixes.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
