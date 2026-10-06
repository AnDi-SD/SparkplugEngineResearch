#pragma once
#include "spInputDeviceAbi.h"

namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t DXMouseClassID = 0x72F650C7;
    inline constexpr std::uint32_t DXMouseFactory = 0x4CC4F0;
    inline constexpr std::uint32_t DXMouseClone = 0x4CC550;
    inline constexpr std::uint32_t DXMouseStartupPrimary = 0x4CC5C0;
    inline constexpr std::uint32_t DXMouseSetExclusivePrimary = 0x4CC470;
    inline constexpr std::uint32_t DXMouseSetCursorVisiblePrimary = 0x4CC480;
    inline constexpr std::uint32_t DXMouseNativeSize = 0x50C8;
    inline constexpr std::uint32_t DXMouseGuidAddress = 0x727EA0;
    inline constexpr std::uint32_t DXMouseDataFormatAddress = 0x712B6C;
    inline constexpr std::uint32_t DXMousePollSecondary = 0x4CC730;
    inline constexpr std::uint32_t DXMouseProcessEventsPrimary = 0x4CC1C0;
    inline constexpr std::uint32_t DXMouseEventBufferOffset = 0x98;
    inline constexpr std::uint32_t DXMouseCurrentPacketOffset = 0x5098;
    inline constexpr std::uint32_t DXMouseChangedPacketOffset = 0x50AC;
    inline constexpr std::uint32_t DXMousePositionOffset = 0x50C0;
    inline constexpr std::uint32_t DXMouseWindowOffset = 0x88;
    inline constexpr std::array<std::uint32_t, 6> DXMousePhysicalSecondary{
        0x4CC320, 0x4CC350, 0x4CC380, 0x4CC430, 0x47A180, 0x4CCA30};
}
