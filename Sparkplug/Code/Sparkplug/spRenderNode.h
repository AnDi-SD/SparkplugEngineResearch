#pragma once

// Inferred declaration path. The class and its serializer are present on PC
// and PS2, but only spRenderNodeSerializer.cpp survives as an exact source
// filename; no original class implementation/header path has been recovered.

#include "spNode.h"
#include "spRenderable.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    // Scene-graph node which owns the renderables consumed by renderer and
    // scene-optimizer traversal. shared_ptr is the portable substitute for
    // the native intrusive references and does not claim native ABI layout.
    class spRenderNode : public spNode
    {
    public:
        static constexpr spClassID ClassID = 0x603625D0;

        spRenderNode() noexcept = default;
        ~spRenderNode() override;

        spRenderNode(const spRenderNode&) = delete;
        spRenderNode& operator=(const spRenderNode&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(
            spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] std::size_t GetRenderableCountForAnalysis() const noexcept;
        [[nodiscard]] spRenderable* GetRenderableForAnalysis(
            std::size_t index) noexcept;
        [[nodiscard]] const spRenderable* GetRenderableForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] bool AttachRenderableForAnalysis(
            std::shared_ptr<spRenderable> renderable);
        [[nodiscard]] std::shared_ptr<spRenderable> DetachRenderableForAnalysis(
            spRenderable& renderable) noexcept;
        void ClearRenderablesForAnalysis() noexcept;

        // Native mutations invalidate aggregate bounds and renderer state.
        // The exact platform caches remain evidence-only; this flag lets
        // portable consumers preserve the proven invalidation boundary.
        [[nodiscard]] bool AreRenderableBoundsDirtyForAnalysis() const noexcept;
        void MarkRenderableBoundsCleanForAnalysis() noexcept;

    private:
        std::vector<std::shared_ptr<spRenderable>> renderables_;
        bool renderableBoundsDirty_ = true;
    };
}
