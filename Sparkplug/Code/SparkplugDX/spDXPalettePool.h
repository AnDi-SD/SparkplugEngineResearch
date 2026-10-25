#pragma once

// Captured renderer palette-index state shared by the renderer operations and
// the texture's real palette-replacement path. This is not a native list ABI.
#include <cstddef>
#include <cstdint>
#include <list>

namespace sparkplug::reconstruction
{
    using DXPaletteEntriesForAnalysis=std::int32_t (*)(void*,std::uintptr_t,
        std::uint32_t,const std::byte*) noexcept;

    struct DXPalettePoolForAnalysis final
    {
        std::uint32_t nextIndex = 0; // native rendererCA10
        std::list<std::uint32_t> freeIndices; // nativeCA14 payload order
        std::size_t maximumFreeIndices = 4096; // HOST bound.
    };
}
