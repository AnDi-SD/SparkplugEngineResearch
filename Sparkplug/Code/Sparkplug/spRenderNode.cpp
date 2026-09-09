#include "spRenderNode.h"

#include <algorithm>
#include <utility>
#include <cstring>
#include "Analysis/PC/spRenderNodeContext.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateRenderNode()
        {
            return std::make_unique<spRenderNode>();
        }

        const spRTTIRecord RenderNodeRecord{
            spRenderNode::ClassID, spNode::ClassID,   "spRenderNode",
            &spNode::StaticRTTI(), &CreateRenderNode, nullptr,
        };

        const bool RenderNodeRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(RenderNodeRecord);
    } // namespace

    bool spRenderNode::PrepareForRenderForAnalysis(evidence::pc::RenderNodeContextForAnalysis& state) noexcept
    {
        if(!state.preserveLightSelection)state.currentLights=&lightCache_;
        UpdateRenderMatricesForAnalysis();
        if(!state.matrices)return false;
        spDXRenderer::MatrixStateForAnalysis::RawMatrix words{};
        std::memcpy(words.data(),cachedMatrix_.data(),sizeof(words));
        if(!spDXRenderer::SetInputMatrixForAnalysis(*state.matrices,0,words,0,state.setMatrix,state.deviceContext))return false;
        state.currentSphere=support_.worldSphere;return true;
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

    std::unique_ptr<spBaseObject> spRenderNode::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spRenderNode>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spRenderNode::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* renderNode = dynamic_cast<spRenderNode*>(&destination);
        // Native424980 copies Node first, then support469FB0. Host rejects
        // self-copy: native self-append traversal is not a bounded operation.
        if (renderNode == nullptr || renderNode == this)
        {
            return false;
        }
        if (!spNode::vfunc_14(destination, manager))
            return false;
        renderNode->support_.localSphere = support_.localSphere;
        renderNode->support_.worldSphere = support_.worldSphere;

        for (const auto& renderable : support_.renderables)
        {
            if (renderable == nullptr)
            {
                return false;
            }

            // PC support calls412BE0 (always clone), NOT map-aware4D3810.
            // Repeated renderable references deliberately become separate
            // model objects; each model still shares its mesh relationship.
            auto cloneBase = manager.Clone(*renderable);
            auto* clone = dynamic_cast<spRenderable*>(cloneBase.get());
            if (clone == nullptr)
            {
                return false;
            }

            std::shared_ptr<spRenderable> ownedClone(
                static_cast<spRenderable*>(cloneBase.release()));
            if (!renderNode->AttachRenderableForAnalysis(std::move(ownedClone)))
                return false;
        }
        // Source spheres are written AGAIN after per-append recomputations.
        // Native copy appends to destination; matrices, dirty134, light cache
        // and scene ownership remain destination state, not source state.
        renderNode->support_.localSphere = support_.localSphere;
        renderNode->support_.worldSphere = support_.worldSphere;
        renderNode->supportControls_ = supportControls_;
        renderNode->cullBypass_ = cullBypass_;
        return true;
    }

    const spRTTIRecord& spRenderNode::vfunc_18() const noexcept
    {
        return RenderNodeRecord;
    }

    std::size_t spRenderNode::GetRenderableCountForAnalysis() const noexcept
    {
        return support_.renderables.size();
    }

    spRenderable* spRenderNode::GetRenderableForAnalysis(const std::size_t index) noexcept
    {
        return index < support_.renderables.size() ? support_.renderables[index].get() : nullptr;
    }

    const spRenderable* spRenderNode::GetRenderableForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < support_.renderables.size() ? support_.renderables[index].get() : nullptr;
    }

    bool spRenderNode::AttachRenderableForAnalysis(std::shared_ptr<spRenderable> renderable)
    {
        return support_.Append(std::move(renderable), cachedMatrix_);
    }

    std::shared_ptr<spRenderable> spRenderNode::DetachRenderableForAnalysis(
        spRenderable& renderable) noexcept
    {
        return support_.DetachForHost(renderable, cachedMatrix_);
    }

    void spRenderNode::ClearRenderablesForAnalysis() noexcept
    {
        support_.ClearForHost(cachedMatrix_);
    }

    bool spRenderNode::AreRenderableBoundsDirtyForAnalysis() const noexcept
    {
        return (flags_ & 2U) != 0;
    }

    void spRenderNode::MarkRenderableBoundsDirtyForAnalysis() noexcept
    {
        flags_ |= 2U;
    }

    void spRenderNode::MarkRenderableBoundsCleanForAnalysis() noexcept
    {
        flags_ &= ~2U;
    }

    void spRenderNode::RebuildRenderableBoundsForAnalysis() noexcept
    {
        support_.Rebuild(cachedMatrix_);
    }

    const spRenderNode::BoundingSphere& spRenderNode::GetLocalBoundingSphereForAnalysis()
        const noexcept
    {
        return support_.localSphere;
    }

    const spRenderNode::BoundingSphere& spRenderNode::GetWorldBoundingSphereForAnalysis()
        const noexcept
    {
        return support_.worldSphere;
    }

    bool spRenderNode::UpdateWorldForAnalysis(const std::uint32_t inheritedFlags,
                                              const Matrix3* cameraOrientation) noexcept
    {
        const auto captured = flags_ | inheritedFlags;
        if (!spNode::UpdateWorldForAnalysis(inheritedFlags, cameraOrientation))
            return false;
        for (std::size_t i = 0; i < 3; ++i)
            reciprocalWorldScale_[i] = 1.F / GetWorldScaleForAnalysis()[i];
        if (flags_ & (BillboardAxis1Mask | BillboardAxis2Mask))
            MarkLocalTransformDirtyForAnalysis(); // arms next frame AFTER capture
        if (captured & 2U)
            RebuildRenderableBoundsForAnalysis();
        if (captured & 1U)
        {
            renderMatricesDirty_ = true;
            support_.worldSphere = evidence::pc::render_node_math::FromPRS(
                support_.localSphere, GetWorldPositionForAnalysis(), GetWorldOrientationForAnalysis(),
                GetWorldScaleForAnalysis());
            if (sceneLightManager_ && supportControls_[3] && IsEnabledForAnalysis())
                sceneLightManager_->RebuildCacheForAnalysis(lightCache_, support_.worldSphere,
                                                            supportControls_[1] != 0);
            // Native424EF0 partition/callback refresh follows here when a
            // Scene exists. That unrepresented ownership graph is not faked.
        }
        return true;
    }

    void spRenderNode::UpdateRenderMatricesForAnalysis() noexcept
    {
        if (!renderMatricesDirty_)
            return;
        cachedMatrix_ = GetWorldMatrixForAnalysis();
        cachedInverse_ = evidence::pc::render_node_math::InversePRS(
            GetWorldPositionForAnalysis(), GetWorldOrientationForAnalysis(), reciprocalWorldScale_);
        renderMatricesDirty_ = false;
    }

    bool spRenderNode::AreRenderMatricesDirtyForAnalysis() const noexcept
    {
        return renderMatricesDirty_;
    }
    const spNode::Matrix4& spRenderNode::GetCachedRenderMatrixForAnalysis() const noexcept
    {
        return cachedMatrix_;
    }
    const spNode::Matrix4& spRenderNode::GetCachedRenderInverseForAnalysis() const noexcept
    {
        return cachedInverse_;
    }
    const spNode::Vector3& spRenderNode::GetReciprocalWorldScaleForAnalysis() const noexcept
    {
        return reciprocalWorldScale_;
    }

    bool spRenderNode::IsCulledForAnalysis(const FrustumPlanes& planes) const noexcept
    {
        return evidence::pc::render_node_math::Culled(support_.worldSphere, planes, cullBypass_);
    }

    void spRenderNode::SetCullBypassForAnalysis(bool value) noexcept
    {
        cullBypass_ = value;
    }
    void spRenderNode::SetSceneLightManagerForAnalysis(const spLightManager* manager) noexcept
    {
        sceneLightManager_ = manager;
    }
    const spLightManager::CacheForAnalysis& spRenderNode::GetLightCacheForAnalysis() const noexcept
    {
        return lightCache_;
    }

    void spRenderNode::SetSupportControlsForAnalysis(
        const std::array<std::uint8_t, 4>& controls) noexcept
    {
        supportControls_ = controls;
    }

    const std::array<std::uint8_t, 4>& spRenderNode::GetSupportControlsForAnalysis() const noexcept
    {
        return supportControls_;
    }
} // namespace sparkplug::reconstruction
