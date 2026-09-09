#pragma once

// Inferred declaration path. PC factory41A7C0 and observed10C object derive
// NamedObject, with common render support and two independently stored matrices.
#include "Analysis/PC/spRenderSupport.h"

namespace sparkplug::reconstruction
{
    class spScene;
    class spStaticRenderObject : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x56D67170;
        using Matrix4 = evidence::pc::render_node_math::Matrix4;
        spStaticRenderObject() noexcept = default;
        ~spStaticRenderObject() override = default;
        spStaticRenderObject(const spStaticRenderObject&) = delete;
        spStaticRenderObject& operator=(const spStaticRenderObject&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Copy remains inherited NamedObject only (PC413120). No vector,
        // matrices or scene are copied into the fresh clone.
        [[nodiscard]] const Matrix4& GetWorldMatrixForAnalysis() const noexcept { return world_; }
        [[nodiscard]] const Matrix4& GetWorldInverseMatrixForAnalysis() const noexcept { return inverse_; }
        // Literal assignments PC44FF57/44FF31. They do not rebuild bounds or
        // derive either matrix from the other; append observes the current world.
        void SetWorldMatrixForAnalysis(const Matrix4& value) noexcept { world_ = value; }
        void SetWorldInverseMatrixForAnalysis(const Matrix4& value) noexcept { inverse_ = value; }
        void SetScenePointerForAnalysis(spScene* scene) noexcept { scene_ = scene; }
        [[nodiscard]] spScene* GetSceneForAnalysis() const noexcept { return scene_; }
        [[nodiscard]] bool AttachRenderableForAnalysis(std::shared_ptr<spRenderable> value)
        { return support_.Append(std::move(value), world_); }
        [[nodiscard]] std::size_t GetRenderableCountForAnalysis() const noexcept
        { return support_.renderables.size(); }
        [[nodiscard]] spRenderable* GetRenderableForAnalysis(std::size_t i) const noexcept
        { return i < support_.renderables.size() ? support_.renderables[i].get() : nullptr; }
        [[nodiscard]] const spRenderable::BoundingSphere& GetLocalBoundingSphereForAnalysis() const noexcept
        { return support_.localSphere; }
        [[nodiscard]] const spRenderable::BoundingSphere& GetWorldBoundingSphereForAnalysis() const noexcept
        { return support_.worldSphere; }
    private:
        evidence::pc::RenderSupportForAnalysis support_;
        spScene* scene_ = nullptr; // borrowed88, no registry operation implied
        // Actual6D38C0 shared identity startup, then constructor copies both.
        Matrix4 world_ = evidence::pc::render_node_math::Identity4;
        Matrix4 inverse_ = evidence::pc::render_node_math::Identity4;
    };
}
