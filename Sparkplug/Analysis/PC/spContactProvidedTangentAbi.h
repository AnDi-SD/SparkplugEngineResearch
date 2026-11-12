#pragma once
#include "spContactRowAbi.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Native method/body offsets only, not a portable native object layout.
    inline constexpr std::uint32_t spContactProvidedTangentOwn20=0x48B610;
    inline constexpr std::size_t spContactProvidedTangentOwn20Bytes=1620;
    inline constexpr std::uint32_t spContactProvidedTangentActualBase=0x48B3C0;
    inline constexpr std::size_t spContactProvidedTangentActualBaseBytes=144;
    inline constexpr std::uint32_t spContactProvidedTangentFlagMask=0x7E5;
    inline constexpr std::uint32_t spContactProvidedTangentBlock=0x48B915;
    inline constexpr std::size_t spContactProvidedTangentBlockBytes=0x6C;
    inline constexpr std::size_t spContactProvidedTangentProjectionBytes=384;
    inline constexpr std::size_t spContactProvidedTangentRowStrideWords=8;
    inline constexpr std::uint32_t spContactProvidedTangentManager7C=0x3E4CCCCD;
    inline constexpr std::uint32_t spContactProvidedTangentManager80=0x3727C5AC;
}
