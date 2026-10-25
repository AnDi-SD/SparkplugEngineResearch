#pragma once

// COM leaves for original4C9D20/4C9D00/4C99A0. Used elements include the
// native terminator; the unused allocator tail is not a valid declaration.
#include "spPCVertexDeclarationElements.h"
#include <cstddef>

namespace sparkplug::reconstruction
{
    struct spPCVertexDeclarationOutputForAnalysis final
    {
        // Original reuses the flags argument's stack slot, then copies it to
        // field18. The initial raw value is flags, not the old COM handle.
        std::uintptr_t value=0;
        bool initialized=false; // provider explicitly wrote valid/null output
    };
    struct spPCVertexDeclarationDeviceForAnalysis final
    {
        using Handle=std::uintptr_t;
        void* context=nullptr;
        std::int32_t (*create)(void*,Handle,const spPCVertexElementForAnalysis*,
            std::size_t,spPCVertexDeclarationOutputForAnalysis&) noexcept=nullptr;
        std::int32_t (*bind)(void*,Handle,Handle) noexcept=nullptr;
        std::uint32_t (*release)(void*,Handle) noexcept=nullptr;
    };
    struct spPCVertexDeclarationResultForAnalysis final
    {
        bool completed=false,nativeReturn=false;
    };
}
