#include "RenderMeshView.h"
#include "ResourceGraph.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include "Analysis/Host/RenderTopology.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace spvhost {
using namespace sparkplug::reconstruction;
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
SpvVertexLayout RenderMeshView::Layout(const spVertexBuffer& vb) {
    const auto flags=vb.GetComponentFlagsForAnalysis();const auto& offsets=vb.GetComponentOffsetsForAnalysis();
    const auto offset=[&](std::uint32_t bit,std::size_t slot)->std::int32_t{return flags&bit?offsets[slot]*4:-1;};
    // Attribute roles are the host's export/render mapping of the original
    // component table. Stride and every byte offset come from that table.
    return {vb.GetVertexStrideForAnalysis(),offset(0x40,7),offset(0x100,9),offset(0x800,12),offset(0x1000,13),
        (flags&0x1e)==0x1e?static_cast<std::int32_t>(offsets[2]*4):-1,offset(0x20,6)};
}
RenderMeshView::RenderMeshView(spStream& stream,std::uint32_t size,std::uint32_t kind,std::uint32_t platformMask) {
    Require(size&&size<=spMeshDataSerializer::MaximumPayloadBytesForAnalysis&&kind<=4,"Invalid bounded render-mesh input");
    metadataOnly=kind>=3;
    renderer=CpuRenderer();std::string error;
    if(kind==0) {
        spSerializerManager manager;spResourceManager resources;
        manager.SetDispatchContextForAnalysis(platformMask,1);
        spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=renderer.get();
        // Use the PC whole selector for PC input. For other platforms the
        // existing portable reader exposes field0 to this modern host backend.
        if(!spMeshDataSerializer().ReadMeshFieldsForAnalysis(context,stream,size,mesh,true,&error,
            [&](const auto& ib,const auto& vb,const auto& observed){Capture(ib,vb,observed);}))throw std::runtime_error(error);
    } else {
        spIndexBuffer ib;spVertexBuffer vb;spMeshDataSerializer::BufferReadObservationForAnalysis observed;
        const bool native=kind==2||kind==4;
        if(!spMeshDataSerializer::ReadBuffersForAnalysis(stream,size,native,ib,vb,&error,&observed))throw std::runtime_error(error);
        Require(mesh.InitializeFromBuffersForAnalysis(ib,vb,false,nullptr,renderer.get()),"Cannot initialize original PC mesh buffers");
        observed.fieldID=native?1:0;Capture(ib,vb,observed);
    }
}
void RenderMeshView::Capture(const spIndexBuffer& ib,const spVertexBuffer& vb,
    const spMeshDataSerializer::BufferReadObservationForAnalysis& observed) {
    const auto layout=Layout(vb);const auto flags=vb.GetComponentFlagsForAnalysis();
    info={observed.fieldID,observed.fieldPayloadOffset,observed.indexPayloadOffset,observed.vertexPayloadOffset,
        static_cast<std::uint32_t>(ib.GetTypeForAnalysis()),ib.GetPrimitiveCountForAnalysis(),vb.GetVertexCountForAnalysis(),
        ib.GetIndexCountForAnalysis(),static_cast<std::uint32_t>(ib.GetIndexElementSizeForAnalysis()),layout.stride,
        mesh.GetVertexStrideForAnalysis(),mesh.GetVertexByteSizeForAnalysis(),flags,0,{},observed.planningByte};
    std::copy(observed.planningWords.begin(),observed.planningWords.end(),info.planningWords);
    // Six exported attribute arrays: normal, color, UV0, UV1, four weights, packed bones.
    const std::int32_t offsets[]={layout.normal,layout.color,layout.uv0,layout.uv1,layout.weights,layout.bones};
    for(unsigned i=0;i<6;++i)if(offsets[i]>=0)info.attributes|=1u<<i;
    if(metadataOnly)return;
    Require((flags&0x1e)==0||(flags&0x1e)==0x1e,"Host mesh view requires absent or four authored blend weights");
    indices.resize(info.indices);
    for(std::uint32_t i=0;i<info.indices;++i) {
        const auto index=ib.GetIndexForAnalysis(i);Require(index.has_value(),"Host mesh view index is missing");
        indices[i]=*index;
    }
    triangleProjectionAvailable=info.primitiveType==2||info.primitiveType==3;
    if(triangleProjectionAvailable)triangleIndices=sparkplug::host::render_topology::Triangles(indices,info.primitiveType,info.vertices);
    else for(const auto index:indices)Require(index<info.vertices,"Host mesh view index is outside its vertex buffer");
    vertices.assign(info.vertices,{});const auto& data=vb.GetDataForAnalysis();
    Require(data.size()==std::uint64_t(info.vertices)*layout.stride,"Vertex buffer size differs from original component layout");
    for(std::uint32_t i=0;i<info.vertices;++i) {
        const auto* bytes=data.data()+std::size_t(i)*layout.stride;auto& target=vertices[i];
        const auto copy=[&](void* out,std::int32_t offset,std::uint32_t size) {
            if(offset<0)return;
            Require(std::uint64_t(offset)+size<=layout.stride,"Host attribute exceeds original vertex stride");
            std::memcpy(out,bytes+offset,size);
        };
        copy(target.position,0,sizeof(target.position));copy(target.normal,layout.normal,sizeof(target.normal));
        copy(target.uv0,layout.uv0,sizeof(target.uv0));copy(target.uv1,layout.uv1,sizeof(target.uv1));
        copy(target.weights,layout.weights,sizeof(target.weights));copy(&target.color,layout.color,4);copy(&target.bones,layout.bones,4);
        // No normalization, inferred weights or other game math in this DTO copy.
        const auto finite=[](const auto& values){return std::all_of(std::begin(values),std::end(values),[](float x){return std::isfinite(x);});};
        Require(finite(target.position)&&finite(target.normal)&&finite(target.uv0)&&finite(target.uv1)&&finite(target.weights),"Non-finite host mesh attribute");
    }
}
static_assert(sizeof(SpvVertexLayout)==28&&sizeof(SpvMeshInfo)==76&&sizeof(SpvMeshVertex)==64);
}
