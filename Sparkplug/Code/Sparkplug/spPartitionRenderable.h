#pragma once
// Inferred path. BaseObject family; actual base factory returns the PC-derived
// resource. Shared render support469ED0/469820, identity760058 after CRT6D38C0.
#include "Analysis/PC/spRenderSupport.h"
namespace sparkplug::reconstruction
{
    class spScene;
    class spPartitionRenderable : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x94BBCA2A;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::uint32_t GetDebugColorForAnalysis() const noexcept{return debugColor_;}
        void SetDebugColorForAnalysis(std::uint32_t value) noexcept{debugColor_=value;}
        [[nodiscard]] bool AttachRenderableForAnalysis(std::shared_ptr<spRenderable> value)
        {return support_.Append(std::move(value),evidence::pc::render_node_math::Identity4);}
        [[nodiscard]] const auto& GetRenderablesForAnalysis() const noexcept{return support_.renderables;}
        [[nodiscard]] const auto& GetLocalBoundingSphereForAnalysis() const noexcept{return support_.localSphere;}
        [[nodiscard]] const auto& GetWorldBoundingSphereForAnalysis() const noexcept{return support_.worldSphere;}
        // Actual4D7260 submits both shared760058 pointers initialized by
        //6D38C0. These analytical getters expose that state, not a new inverse.
        [[nodiscard]] const evidence::pc::render_node_math::Matrix4& GetWorldMatrixForAnalysis() const noexcept
        {return evidence::pc::render_node_math::Identity4;}
        [[nodiscard]] const evidence::pc::render_node_math::Matrix4& GetWorldInverseMatrixForAnalysis() const noexcept
        {return evidence::pc::render_node_math::Identity4;}
        // Borrowed pointer assignment only, not Scene attachment/initialization.
        void SetScenePointerForAnalysis(spScene* scene) noexcept{scene_=scene;}
        [[nodiscard]] spScene* GetSceneForAnalysis() const noexcept{return scene_;}
    protected:
        spPartitionRenderable() noexcept=default;
    private:
        evidence::pc::RenderSupportForAnalysis support_;
        std::uint32_t debugColor_=0xFF000000;
        spScene* scene_=nullptr;
    };
}
