// Continuation of original spOcclusionVolume.cpp, split for incremental builds.
#include "spOcclusionVolume.h"
#include "../../Analysis/PC/spVertexBounds.h"
#include <algorithm>
#include <cstring>

namespace sparkplug::reconstruction {
bool spOcclusionVolume::InitializeForAnalysis(const spIndexBuffer& inputIndices,
    const spVertexBuffer& inputVertices,const SortDispatchForAnalysis& sort,std::string* error)
{
    const auto fail=[&](const char* text){if(error)*error=text;return false;};
    if(error)error->clear();
    if(initializationAttempted_||shapeBuffersPrepared_||!faces_.empty()||!edges_.empty())
        return fail("Occlusion requires a fresh object; repeated Init has unsupported original lifetime");
    const auto count=inputVertices.GetVertexCountForAnalysis(),indexCount=inputIndices.GetIndexCountForAnalysis();
    const std::size_t stride=inputVertices.GetVertexStrideForAnalysis();
    const std::size_t positionOffset=inputVertices.GetComponentOffsetsForAnalysis()[0]*4;
    const auto& source=inputVertices.GetDataForAnalysis();
    if(!sort.invoke||!inputIndices.IsInitializedForAnalysis()||!inputVertices.IsInitializedForAnalysis()||
        inputIndices.GetTypeForAnalysis()!=spIndexBuffer::eIndexBufferType::Type2||
        !count||count>4096||indexCount<6||indexCount>3072||indexCount%3||
        positionOffset>stride||stride-positionOffset<12||count>source.size()/stride)
        return fail("Unsupported Occlusion buffers, single-triangle shape or missing sort dependency");
    std::vector<std::byte> positions(count*12);
    for(std::size_t i=0;i<count;++i) {
        std::memcpy(positions.data()+i*12,source.data()+i*stride+positionOffset,12);
        Vector3 value;std::memcpy(value.data(),positions.data()+i*12,12);
        for(auto component:value)if(!std::isfinite(component))return fail("Occlusion requires finite positions");
    }
    for(std::uint32_t i=0;i<indexCount;++i) {
        const auto value=inputIndices.GetIndexForAnalysis(i);
        // PC471060 reads only the low WORD of a UInt32 index. High bits are
        // deliberately not a host refusal when the truncated reference is valid.
        if(!value||static_cast<std::uint16_t>(*value)>=count)return fail("Truncated Occlusion index is outside its vertex buffer");
    }
    initializationAttempted_=true;
    if(inputIndices.GetFormatFlagsForAnalysis()&1) {
        indices_=std::make_unique<spIndexBuffer>();
        if(!indices_->InitializeForAnalysis(inputIndices.GetPrimitiveCountForAnalysis(),spIndexBuffer::eIndexBufferType::Type2,0))return fail("Occlusion IB allocation failed");
        for(std::uint32_t i=0;i<indexCount;++i)
            if(!indices_->SetIndexForAnalysis(i,static_cast<std::uint16_t>(*inputIndices.GetIndexForAnalysis(i))))return fail("Occlusion IB copy failed");
    } else indices_=inputIndices.CopyBufferForAnalysis(); // PC45FD90
    localVertices_=std::make_unique<spVertexBuffer>();
    if(!indices_||!localVertices_->InitializeFromDataForAnalysis(0,count,0,positions))return fail("Occlusion position-only copy failed");
    // 4711B8 uses the caller's original VB, including vertices later discarded
    // by welding/compaction. Keep a position-only view with identical points.
    spVertexBuffer sphereInput;
    if(!sphereInput.InitializeFromDataForAnalysis(0,count,0,positions))return fail("Occlusion input position view failed");
    evidence::pc::GeometryHelper4604F0 helper;
    evidence::pc::GeometryHelper4604F0::ObservationForAnalysis observation;
    if(!helper.WeldForAnalysis(*indices_,*localVertices_,sort,observation,error))return false;
    std::vector<Vector3> world(localVertices_->GetVertexCountForAnalysis());
    for(std::size_t i=0;i<world.size();++i)std::memcpy(world[i].data(),localVertices_->GetDataForAnalysis().data()+12*i,12);
    std::vector<std::uint16_t> triangles(indexCount);
    for(std::uint32_t i=0;i<indexCount;++i)triangles[i]=static_cast<std::uint16_t>(*indices_->GetIndexForAnalysis(i));
    if(!SetShapeBuffersForAnalysis(std::move(world),std::move(triangles),walkStamp_)||!BuildShapeForAnalysis())
        return fail("Original Occlusion shape did not initialize in the supported host slice");
    if(!evidence::pc::ComputeVertexSphere(sphereInput,localSphere_))return fail("Occlusion sphere input failed");
    // PC4711BD starts min and max at zero; this is not vertex-only bounds.
    localBounds_.fill(0);
    for(std::size_t i=0;i<count;++i) {
        Vector3 point;std::memcpy(point.data(),positions.data()+12*i,12);
        for(std::size_t c=0;c<3;++c) {
            if(point[c]<localBounds_[c])localBounds_[c]=point[c];
            if(point[c]>localBounds_[c+3])localBounds_[c+3]=point[c];
        }
    }
    cameraDirty_=initialized_=true;return true;
}
bool spOcclusionVolume::UpdateWorldForAnalysis(std::uint32_t inheritedFlags,const Matrix3* cameraOrientation) noexcept
{
    const auto captured=GetFlagsForAnalysis()|inheritedFlags;
    if(!spNode::UpdateWorldForAnalysis(inheritedFlags,cameraOrientation))return false;
    if(!(captured&1))return true;
    cameraDirty_=true;
    if(initialized_)for(std::size_t i=0;i<points_.size();++i) {
        Vector3 local;std::memcpy(local.data(),localVertices_->GetDataForAnalysis().data()+12*i,12);
        points_[i]=TransformPointToWorldForAnalysis(local);
    }
    const auto& s=GetWorldScaleForAnalysis();const auto& m=GetWorldOrientationForAnalysis();const auto& p=GetWorldPositionForAnalysis();
    const double x=double(localSphere_[0])*s[0],y=double(localSphere_[1])*s[1],z=double(localSphere_[2])*s[2];
    // 46DEB1/46DED1 round X/Y rotation before translation; Z remains extended
    // until 46DF02. This is distinct from Node420660's all-three float stores.
    worldSphere_={static_cast<float>(double(static_cast<float>((z*m[6]+y*m[3])+x*m[0]))+p[0]),
        static_cast<float>(double(static_cast<float>((z*m[7]+y*m[4])+x*m[1]))+p[1]),
        static_cast<float>(((z*m[8]+y*m[5])+x*m[2])+p[2]),
        static_cast<float>(double(localSphere_[3])*std::max({std::abs(s[0]),std::abs(s[1]),std::abs(s[2])}))};
    // Original Scene partition notifications at46DF1C are outside the loaded
    // tools graph. No synthetic Scene registration or silhouette is produced.
    return true;
}
}
