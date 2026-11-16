#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::moving_platform
{
    inline constexpr std::uint32_t ClassId = 0x69DA1E47;
    inline constexpr std::uint32_t PrimaryTable = 0x00701048;
    inline constexpr std::uint32_t RttiRecord = 0x00754AE8;
    inline constexpr std::uint32_t NearStateOperation = 0x0057E7E0;
    inline constexpr std::uint32_t PristineGlobalAddress = 0x00765AB8;
    inline constexpr std::size_t CapturedPrefixBytes = 0x169;
    inline constexpr std::size_t NearByteOffset = 0x168;
    inline constexpr std::size_t PointPositionOffset = 0x18;
    // The historical factory allocation measured364 bytes. CapturedPrefixBytes
    // is only this operation's minimum touched prefix,not native sizeof.
}
