#pragma once
// Host renderer lifetime and transport. The material algorithms are the
// reconstructed DXRenderer slices; this adapter never calls Direct3D.
#include "ResourceGraph.h"
#include "ViewerBridge.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXMaterial.h"

namespace spvhost {
class MaterialSubmission final {
public:
    explicit MaterialSubmission(std::shared_ptr<ResourceGraph>);
    void Capture(std::uint32_t material,std::uint32_t frame,
        SpvMaterialDrawPass* output,std::uint32_t capacity,std::uint32_t* count);
    void CaptureTextDefault(sparkplug::reconstruction::spDXMaterial&,std::uint32_t frame,
        SpvMaterialDrawPass* output,std::uint32_t capacity,std::uint32_t* count);
private:
    using Renderer=sparkplug::reconstruction::spDXRenderer;
    void CaptureObject(sparkplug::reconstruction::spDXMaterial*,std::uint32_t,
        SpvMaterialDrawPass*,std::uint32_t,std::uint32_t*,Renderer::UnassignedPowerPolicyForAnalysis);
    std::shared_ptr<ResourceGraph> graph_;
    std::unique_ptr<sparkplug::reconstruction::spDXMaterial> fallback_;
    Renderer::SubmissionStateForAnalysis state_;
    SpvMaterialDrawPass current_{};
    static std::int32_t Render(void*,std::uint32_t,std::uint32_t) noexcept;
    static std::int32_t RenderCached(void*,std::uint32_t,std::uint32_t) noexcept;
    static std::int32_t Texture(void*,std::uint32_t,std::uintptr_t) noexcept;
    static std::int32_t Palette(void*,std::uint32_t) noexcept;
    static std::int32_t TextureState(void*,bool,std::uint32_t,std::uint32_t,std::uint32_t) noexcept;
    static std::int32_t Transform(void*,std::uint32_t,const Renderer::TextureMatrix4ForAnalysis&) noexcept;
    static bool UV(void*,std::uint32_t,const Renderer::TextureMatrix3ForAnalysis&);
};
}
