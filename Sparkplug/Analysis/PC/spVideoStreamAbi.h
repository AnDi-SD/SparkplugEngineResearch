#pragma once
#include <cstddef>
#include <cstdint>
namespace sparkplug::evidence::pc
{
    struct VideoStreamLayout32 final
    {
        std::uint32_t primaryInterface;
        std::uint8_t crossPlatformNamed[0x14];
        std::uint8_t byte18, byte19, unwrittenPadding1A[2];
        std::uint32_t ownedBuffer1C;
        std::uint32_t word20;
    };
    static_assert(sizeof(VideoStreamLayout32) == 0x24);
    static_assert(offsetof(VideoStreamLayout32, crossPlatformNamed) == 4);
    static_assert(offsetof(VideoStreamLayout32, byte18) == 0x18);
    static_assert(offsetof(VideoStreamLayout32, ownedBuffer1C) == 0x1C);
    inline constexpr std::uint32_t VideoStreamPrimaryVtable = 0x6F3580;
    inline constexpr std::uint32_t PCVideoStreamPrimaryVtable = 0x6F2AC4;
    inline constexpr std::uint32_t PCVideoStreamSecondaryVtable = 0x6F2AA8;
    inline constexpr std::uint32_t PCVideoStreamFactory = 0x4C7340;
    inline constexpr std::uint32_t PCVideoStreamCloneSecondary = 0x4C73B0;
    inline constexpr std::uint32_t PCVideoStreamDeletingDestructor = 0x4C7410;
}
