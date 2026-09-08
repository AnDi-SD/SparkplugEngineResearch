#pragma once
// Host view of reconstructed mesh readers and CPU buffers. No GPU API calls.
#include "ViewerBridge.h"
#include "Code/Sparkplug/spMeshDataSerializer.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include <memory>
#include <vector>
namespace spvhost {
class RenderMeshView final {
public:
    RenderMeshView(sparkplug::reconstruction::spStream&,std::uint32_t size,std::uint32_t kind,std::uint32_t platformMask);
    SpvMeshInfo info{};
    std::vector<SpvMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    static SpvVertexLayout Layout(const sparkplug::reconstruction::spVertexBuffer&);
private:
    bool metadataOnly=false;
    std::shared_ptr<sparkplug::reconstruction::spPCRenderer> renderer;
    sparkplug::reconstruction::spDXMesh mesh;
    void Capture(const sparkplug::reconstruction::spIndexBuffer&,const sparkplug::reconstruction::spVertexBuffer&,
        const sparkplug::reconstruction::spMeshDataSerializer::BufferReadObservationForAnalysis&);
};
}
