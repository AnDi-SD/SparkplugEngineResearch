#include "spRenderNode.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderNode()
        {
            return std::make_unique<spRenderNode>();
        }

        const spRTTIRecord RenderNodeRecord{
            spRenderNode::ClassID,
            spNode::ClassID,
            "spRenderNode",
            &spNode::StaticRTTI(),
            &CreateRenderNode,
            nullptr,
        };

        const bool RenderNodeRegistered =
            spRTTIManager::Instance().Register(RenderNodeRecord);
    }

    spRenderNode::~spRenderNode()
    {
        ClearRenderablesForAnalysis();
    }

    const spRTTIRecord& spRenderNode::StaticRTTI() noexcept
    {
        (void)RenderNodeRegistered;
        return RenderNodeRecord;
    }

    std::unique_ptr<spBaseObject> spRenderNode::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderNode>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spRenderNode::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* renderNode = dynamic_cast<spRenderNode*>(&destination);
        if (renderNode == nullptr)
        {
            return false;
        }

        std::vector<std::shared_ptr<spRenderable>> clonedRenderables;
        clonedRenderables.reserve(renderables_.size());
        std::unordered_map<const spRenderable*, std::shared_ptr<spRenderable>>
            localClones;

        for (const auto& renderable : renderables_)
        {
            if (renderable == nullptr)
            {
                return false;
            }

            const auto known = localClones.find(renderable.get());
            if (known != localClones.end())
            {
                clonedRenderables.push_back(known->second);
                continue;
            }

            auto cloneBase = renderable->vfunc_10(manager);
            auto* clone = dynamic_cast<spRenderable*>(cloneBase.get());
            if (clone == nullptr)
            {
                return false;
            }

            std::shared_ptr<spRenderable> ownedClone(
                static_cast<spRenderable*>(cloneBase.release()));
            localClones.emplace(renderable.get(), ownedClone);
            clonedRenderables.push_back(std::move(ownedClone));
        }

        if (!spNode::vfunc_14(destination, manager))
        {
            return false;
        }

        renderNode->renderables_ = std::move(clonedRenderables);
        renderNode->renderableBoundsDirty_ = renderableBoundsDirty_;
        return true;
    }

    const spRTTIRecord& spRenderNode::vfunc_18() const noexcept
    {
        return RenderNodeRecord;
    }

    std::size_t spRenderNode::GetRenderableCountForAnalysis() const noexcept
    {
        return renderables_.size();
    }

    spRenderable* spRenderNode::GetRenderableForAnalysis(
        const std::size_t index) noexcept
    {
        return index < renderables_.size() ? renderables_[index].get() : nullptr;
    }

    const spRenderable* spRenderNode::GetRenderableForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < renderables_.size() ? renderables_[index].get() : nullptr;
    }

    bool spRenderNode::AttachRenderableForAnalysis(
        std::shared_ptr<spRenderable> renderable)
    {
        if (renderable == nullptr)
        {
            return false;
        }
        renderables_.push_back(std::move(renderable));
        renderableBoundsDirty_ = true;
        return true;
    }

    std::shared_ptr<spRenderable> spRenderNode::DetachRenderableForAnalysis(
        spRenderable& renderable) noexcept
    {
        const auto iterator = std::find_if(
            renderables_.begin(),
            renderables_.end(),
            [&renderable](const auto& candidate)
            {
                return candidate.get() == &renderable;
            });
        if (iterator == renderables_.end())
        {
            return nullptr;
        }

        auto detached = std::move(*iterator);
        renderables_.erase(iterator);
        renderableBoundsDirty_ = true;
        return detached;
    }

    void spRenderNode::ClearRenderablesForAnalysis() noexcept
    {
        if (!renderables_.empty())
        {
            renderables_.clear();
            renderableBoundsDirty_ = true;
        }
    }

    bool spRenderNode::AreRenderableBoundsDirtyForAnalysis() const noexcept
    {
        return renderableBoundsDirty_;
    }

    void spRenderNode::MarkRenderableBoundsCleanForAnalysis() noexcept
    {
        renderableBoundsDirty_ = false;
    }
}
