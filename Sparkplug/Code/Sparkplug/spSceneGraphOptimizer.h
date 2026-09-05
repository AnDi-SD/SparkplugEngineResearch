#pragma once

// Inferred declaration path. Both executables register the class in the
// Sparkplug module, but no original header or implementation path survives.

#include "../SparkBase/spBaseObject.h"
#include "spNode.h"

#include <cstddef>
#include <vector>

namespace sparkplug::reconstruction
{
    class spModel;
    class spRenderNode;

    // Common scene traversal used by the PC DX batching implementation.
    // The native class has no RTTI factory on either platform. The callback
    // names and argument roles are preserved from its exact diagnostics;
    // their placement in a secondary native subobject is ABI evidence only.
    class spSceneGraphOptimizer : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x4FE639C2;

        ~spSceneGraphOptimizer() override;

        spSceneGraphOptimizer(const spSceneGraphOptimizer&) = delete;
        spSceneGraphOptimizer& operator=(const spSceneGraphOptimizer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x004C1900: starts the callback pass, traverses the graph,
        // detaches the empty render leaves collected during that pass and
        // always calls OnEndOptimize after a successful OnStartOptimize.
        [[nodiscard]] bool OptimizeForAnalysis(spNode& root);

    protected:
        spSceneGraphOptimizer() noexcept = default;

        [[nodiscard]] virtual bool OnStartOptimize(spNode& root) = 0;
        [[nodiscard]] virtual bool OnEndOptimize() = 0;
        [[nodiscard]] virtual bool OnNode(spNode& node) = 0;
        [[nodiscard]] virtual bool OnModel(
            spRenderNode& renderNode,
            spModel& model) = 0;

    private:
        [[nodiscard]] bool OptimizeNodeForAnalysis(
            spNode& node,
            std::size_t& currentNode);

        // Safe host substitute for the native temporary intrusive list.
        std::vector<spRenderNode*> emptyRenderLeaves_;
    };
}
