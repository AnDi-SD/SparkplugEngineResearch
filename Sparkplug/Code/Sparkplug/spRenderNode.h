#pragma once

// Inferred declaration path. The class and its serializer are present on PC
// and PS2, but only spRenderNodeSerializer.cpp survives as an exact source
// filename; no original class implementation/header path has been recovered.

#include "spNode.h"
#include "spRenderable.h"
#include "spLightManager.h"
#include "Analysis/PC/spRenderNodeMath.h"
#include "Analysis/PC/spRenderSupport.h"

#include <cstddef>
#include <memory>
#include <vector>
namespace sparkplug::evidence::pc {struct RenderNodeContextForAnalysis;}

namespace sparkplug::reconstruction
{
    // Scene-graph node which owns the renderables consumed by renderer and
    // scene-optimizer traversal. shared_ptr is the portable substitute for
    // the native intrusive references and does not claim native ABI layout.
    class spRenderNode : public spNode
    {
      public:
        static constexpr spClassID ClassID = 0x603625D0;
        using BoundingSphere = spRenderable::BoundingSphere;
        using FrustumPlanes = evidence::pc::render_node_math::Planes;

        spRenderNode() noexcept = default;
        ~spRenderNode() override;

        spRenderNode(const spRenderNode&) = delete;
        spRenderNode& operator=(const spRenderNode&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] std::size_t GetRenderableCountForAnalysis() const noexcept;
        [[nodiscard]] spRenderable* GetRenderableForAnalysis(std::size_t index) noexcept;
        [[nodiscard]] const spRenderable* GetRenderableForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] bool AttachRenderableForAnalysis(std::shared_ptr<spRenderable> renderable);
        [[nodiscard]] std::shared_ptr<spRenderable> DetachRenderableForAnalysis(
            spRenderable& renderable) noexcept;
        void ClearRenderablesForAnalysis() noexcept;

        // Native469ED0 append recomputes bounds immediately through the OLD
        // cached matrix, without changing dirty flags. These helpers expose
        // the independent Node bit2 used when existing geometry changes.
        [[nodiscard]] bool AreRenderableBoundsDirtyForAnalysis() const noexcept;
        void MarkRenderableBoundsDirtyForAnalysis() noexcept;
        void MarkRenderableBoundsCleanForAnalysis() noexcept;

        void RebuildRenderableBoundsForAnalysis() noexcept;
        [[nodiscard]] const BoundingSphere& GetLocalBoundingSphereForAnalysis() const noexcept;
        [[nodiscard]] const BoundingSphere& GetWorldBoundingSphereForAnalysis() const noexcept;
        [[nodiscard]] bool UpdateWorldForAnalysis(
            std::uint32_t inheritedFlags = 0,
            const Matrix3* cameraOrientation = nullptr) noexcept override;

        // Local cache portion of4248D0. Does not submit a device call or claim
        // to publish renderer-owned current-light/sphere state.
        void UpdateRenderMatricesForAnalysis() noexcept;
        // Actual4248D0 publishes lights before matrix submission and the sphere
        // only after its success. Context is explicit PC renderer state.
        [[nodiscard]] bool PrepareForRenderForAnalysis(evidence::pc::RenderNodeContextForAnalysis&) noexcept;
        [[nodiscard]] bool AreRenderMatricesDirtyForAnalysis() const noexcept;
        [[nodiscard]] const Matrix4& GetCachedRenderMatrixForAnalysis() const noexcept;
        [[nodiscard]] const Matrix4& GetCachedRenderInverseForAnalysis() const noexcept;
        [[nodiscard]] const Vector3& GetReciprocalWorldScaleForAnalysis() const noexcept;
        [[nodiscard]] bool IsCulledForAnalysis(const FrustumPlanes& planes) const noexcept;
        void SetCullBypassForAnalysis(bool value) noexcept;

        // Explicit borrowed scene dependency; this does NOT create/register a
        // Scene or implement its partition/occlusion notifications. Native
        // world-transform dirty branch rebuilds only if control123 && Enabled.
        void SetSceneLightManagerForAnalysis(const spLightManager* manager) noexcept;
        [[nodiscard]] const spLightManager::CacheForAnalysis& GetLightCacheForAnalysis()
            const noexcept;
        // Mutable cache identity for explicit scene refresh target binding.
        [[nodiscard]] spLightManager::CacheForAnalysis& GetLightCacheForAnalysis() noexcept
        { return lightCache_; }
        void SetSupportControlsForAnalysis(const std::array<std::uint8_t, 4>& controls) noexcept;
        [[nodiscard]] const std::array<std::uint8_t, 4>& GetSupportControlsForAnalysis()
            const noexcept;

      private:
        evidence::pc::RenderSupportForAnalysis support_;
        Matrix4 cachedMatrix_ = evidence::pc::render_node_math::Identity4;
        Matrix4 cachedInverse_ = evidence::pc::render_node_math::Identity4;
        Vector3 reciprocalWorldScale_{1, 1, 1};
        bool renderMatricesDirty_ = false;
        bool cullBypass_ = false;
        std::array<std::uint8_t, 4> supportControls_{0, 1, 1, 1}; // native120..123
        spLightManager::CacheForAnalysis lightCache_;
        const spLightManager* sceneLightManager_ = nullptr;
    };
} // namespace sparkplug::reconstruction
