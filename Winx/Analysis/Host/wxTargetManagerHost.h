#pragma once
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spResourceManager.h"
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    class wxAssetManager;

    // Borrowed live input for the single byte read by PC59DF70. This record
    // is not a native entity layout or a recovered C++ class declaration.
    // Pointer identity supplies the native argument word; no reference is owned.
    struct wxTargetCandidateBindingForAnalysis final
    {
        std::uint8_t flag148 = 0;
    };

    // Portable resource ownership, separate from the native intrusive word8.
    // All native owners of a binding must be represented in referenceCount8;
    // shared_ptr::use_count is never interpreted as that native count. A zero
    // reached by native Release clears node through the real source destructor.
    // For actual deletion qualification, graph edges and this canonical node
    // field are its only strong C++ owners; service caches retain bindings or
    // weak_ptrs, never an additional hidden strong Node owner.
    struct wxTargetNodeBindingForAnalysis final
    {
        std::shared_ptr<sparkplug::reconstruction::spNode> node;
        std::uint16_t referenceCount8 = 0;
    };

    class wxTargetManagerHost
    {
    public:
        virtual ~wxTargetManagerHost() = default;
        virtual wxAssetManager* ResolveTargetAssetManagerForAnalysis() = 0;
        virtual sparkplug::reconstruction::spResourceManager* ResolveTargetResourceManagerForAnalysis() = 0;
        // Original458260 is a foreign resource-loading operation. The supplied
        // synchronous name is built by the real shared wxAssetManager body.
        // One canonical binding represents each Node identity. This slice
        // admits actual plain Nodes, stable binding storage and a
        // null native Scene association. AddRef must not wrap; scene-edge
        // detach must leave the manager reference alive until its own Release.
        // Loading/filesystem and inconsistent intrusive inputs are not recovered.
        virtual std::shared_ptr<wxTargetNodeBindingForAnalysis> LoadTargetNodeForAnalysis(
            sparkplug::reconstruction::spResourceManager&, const char* name) = 0;
        // Borrowed engine->current Scene->root prefix. Its graph outlives every
        // manager. Actual Attach/Detach execute directly in the common Node.
        virtual sparkplug::reconstruction::spNode* ResolveTargetSceneRootForAnalysis() = 0;
    };
}
