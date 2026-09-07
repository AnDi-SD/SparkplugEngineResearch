#include "spSceneGraphOptimizer.h"

#include "spModel.h"
#include "spRenderNode.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord SceneGraphOptimizerRecord{
            spSceneGraphOptimizer::ClassID,
            spCrossPlatform::ClassID,
            "spSceneGraphOptimizer",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool SceneGraphOptimizerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SceneGraphOptimizerRecord);
    }

    spSceneGraphOptimizer::~spSceneGraphOptimizer() = default;

    const spRTTIRecord& spSceneGraphOptimizer::StaticRTTI() noexcept
    {
        (void)SceneGraphOptimizerRegistered;
        return SceneGraphOptimizerRecord;
    }

    const spRTTIRecord& spSceneGraphOptimizer::vfunc_18() const noexcept
    {
        return SceneGraphOptimizerRecord;
    }

    bool spSceneGraphOptimizer::OptimizeForAnalysis(spNode& root)
    {
        emptyRenderLeaves_.clear();
        if (!OnStartOptimize(root))
        {
            return false;
        }

        std::size_t currentNode = 0;
        const bool traversalSucceeded =
            OptimizeNodeForAnalysis(root, currentNode);

        // Native code performs this deferred detach pass even when traversal
        // failed after OnStartOptimize. Only leaves reached successfully have
        // entered the list at that point.
        for (auto* renderNode : emptyRenderLeaves_)
        {
            if (renderNode == nullptr)
            {
                continue;
            }
            auto* parent = renderNode->GetParentForAnalysis();
            if (parent != nullptr)
            {
                (void)parent->DetachChildForAnalysis(*renderNode);
            }
        }

        const bool endSucceeded = OnEndOptimize();
        emptyRenderLeaves_.clear();
        return endSucceeded && traversalSucceeded;
    }

    bool spSceneGraphOptimizer::OptimizeNodeForAnalysis(
        spNode& node,
        std::size_t& currentNode)
    {
        // The native signature and diagnostic name this reference
        // o_iCurrentNode, but the recovered common body only forwards it to
        // recursive calls. Do not invent a counter update.
        (void)currentNode;
        if (!OnNode(node))
        {
            return false;
        }

        if (auto* renderNode = dynamic_cast<spRenderNode*>(&node))
        {
            const auto renderableCount =
                renderNode->GetRenderableCountForAnalysis();
            for (std::size_t index = 0; index < renderableCount; ++index)
            {
                auto* model = dynamic_cast<spModel*>(
                    renderNode->GetRenderableForAnalysis(index));
                if (model != nullptr && !OnModel(*renderNode, *model))
                {
                    return false;
                }
            }
        }

        const auto childCount = node.GetChildCountForAnalysis();
        for (std::size_t index = 0; index < childCount; ++index)
        {
            auto* child = node.GetChildForAnalysis(index);
            if (child == nullptr
                || !OptimizeNodeForAnalysis(*child, currentNode))
            {
                return false;
            }
        }

        if (node.GetChildCountForAnalysis() == 0)
        {
            auto* renderNode = dynamic_cast<spRenderNode*>(&node);
            if (renderNode != nullptr
                && renderNode->GetRenderableCountForAnalysis() == 0)
            {
                emptyRenderLeaves_.push_back(renderNode);
            }
        }
        return true;
    }
}
