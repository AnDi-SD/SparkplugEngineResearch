#pragma once

#include "Code/SparkplugPC/spPCErrorManager.h"

namespace sparkplug::reconstruction
{
    class spSerializerManager;
    class spMaterialSerializer;
    class spFogSerializer;
    class spRenderNodeSerializer;

    // Caller-owned analytical dependencies for the bounded PC null-reference
    // Index path. This is not a native structure or a global startup provider.
    // All objects and the nonalias raw word must stay live and stable for the
    // synchronous operation. No cold acquisition or queued handler is admitted.
    struct spProjectionNullIndexContextForAnalysis final
    {
        spSerializerManager& manager;
        spPCErrorManager& errors;
        std::uint32_t& rawWord75AB9C;
        spPCErrorManager::WarmEmptyContextForAnalysis lifetime =
            spPCErrorManager::WarmEmptyContextForAnalysis::UnqualifiedMutable;
    };

    // Separate host admission for exact already-indexed typed relationships.
    // These borrowed serializer bindings and all input objects stay live and
    // stable; rawWord must not alias them. IDs7/9/11 and unwritten entries are
    // restrictions of this supplied profile, not universal native requirements.
    struct spProjectionPreindexedIndexContextForAnalysis final
    {
        spSerializerManager& manager;
        spPCErrorManager& errors;
        std::uint32_t& rawWord75AB9C;
        spMaterialSerializer& materialSerializer;
        spFogSerializer& fogSerializer;
        spRenderNodeSerializer& nodeSerializer;
        spPCErrorManager::WarmEmptyContextForAnalysis lifetime =
            spPCErrorManager::WarmEmptyContextForAnalysis::UnqualifiedMutable;
    };

    // Separate supplied empty-or-ID1 Fog-only FAT profile. Exactly one Fog
    // serializer registration is required; no native address/allocator/ABI
    // mapping is implied. All borrowed inputs stay live/stable, raw nonalias.
    struct spProjectionFogOnlyIndexContextForAnalysis final
    {
        spSerializerManager& manager;
        spPCErrorManager& errors;
        std::uint32_t& rawWord75AB9C;
        spFogSerializer& fogSerializer;
        spPCErrorManager::WarmEmptyContextForAnalysis lifetime =
            spPCErrorManager::WarmEmptyContextForAnalysis::UnqualifiedMutable;
    };

    // Separate supplied empty-Material profile. The exact Material binding,
    // target and final zero-pass/null-controller Material stay stable/live;
    // rawWord does not alias their storage. No adapter/context is retained.
    struct spProjectionEmptyMaterialIndexContextForAnalysis final
    {
        spSerializerManager& manager;
        spPCErrorManager& errors;
        std::uint32_t& rawWord75AB9C;
        spMaterialSerializer& materialSerializer;
        spPCErrorManager::WarmEmptyContextForAnalysis lifetime =
            spPCErrorManager::WarmEmptyContextForAnalysis::UnqualifiedMutable;
    };
}
