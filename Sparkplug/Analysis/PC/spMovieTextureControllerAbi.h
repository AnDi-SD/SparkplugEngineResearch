#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t MovieTextureControllerClassId = 0x7487689D;
    inline constexpr std::uint32_t MovieTextureControllerPrimary = 0x006ECB68;
    inline constexpr std::uint32_t MovieTextureControllerApply = 0x00492EF0;
    inline constexpr std::size_t MovieTextureControllerApplySlot = 0x1C;
    inline constexpr std::size_t MovieTextureControllerMaterial = 0x1C;
    inline constexpr std::uint32_t MovieTextureMaterialPrimary = 0x006EA94C;
    inline constexpr std::size_t MovieTextureMaterialVideo = 0x70;
    inline constexpr std::size_t MovieTextureVideoGate = 0x18;
    inline constexpr std::uint32_t MovieTextureVideoSlot00 = 0x004C7320;
    inline constexpr std::uint32_t MovieTextureVideoSlot04 = 0x0048EAA0;
} // namespace sparkplug::evidence::pc
