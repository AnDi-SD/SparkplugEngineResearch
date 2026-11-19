#pragma once
#include <cstddef>
#include <cstdint>

namespace wxLeverAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00700910;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 296;
inline constexpr std::size_t ActionRawDwordOffset = 0x124;
inline constexpr std::uint32_t ActionGetterPC = 0x00555050;
// MOV EAX, DWORD PTR [ECX+0x124]; RET. No setter or value interpretation.
}
