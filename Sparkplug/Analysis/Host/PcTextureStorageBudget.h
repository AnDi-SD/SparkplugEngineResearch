#pragma once

#include <cstdint>

namespace sparkplug::host::pc_texture_storage
{
    // Host bounds, not original file-format limits. 3000x3000 BGRA32 was
    // previously authored and exercised in the PC game (Flora Enchantix).
    inline constexpr std::uint32_t DefaultBytes = 16u * 1024u * 1024u;
    inline constexpr std::uint32_t ToolResourceGraphBytes = 64u * 1024u * 1024u;
    inline constexpr std::uint32_t BgraBytes = 3000u * 3000u * 4u;
    inline constexpr std::uint32_t BgraChainBytes = BgraBytes * 4u / 3u;
    inline constexpr std::uint32_t BgraSectionBytes = BgraBytes + 4096u;
}
