#pragma once
#include <cstddef>
#include <cstdint>

namespace wxParticleCtrlAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00701C8C;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 296;
inline constexpr std::size_t AnimationFileRawDwordOffset = 0x124;
inline constexpr std::uint32_t AnimationFileGetterPC = 0x00555050;
// MOV EAX, DWORD PTR [ECX+0x124]; RET. No setter or value interpretation.
}
