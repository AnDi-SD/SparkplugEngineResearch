#include "Code/Sparkplug/spOcclusionVolume.h"
#include "Analysis/PC/spOcclusionCameraMath.h"
#include "Analysis/Host/LegacySortPolicy.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
using Volume=spOcclusionVolume;
namespace math=sparkplug::evidence::pc::occlusion_camera_math;
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
float Float(std::uint32_t value){float result;std::memcpy(&result,&value,4);return result;}
std::uint32_t Bits(float value){std::uint32_t result;std::memcpy(&result,&value,4);return result;}
template<std::size_t N> std::array<float,N> ReadFloats()
{
    std::array<float,N> result;for(auto& value:result){std::uint32_t word;if(!(std::cin>>word))throw std::runtime_error("short raw float input");value=Float(word);}return result;
}
template<std::size_t N> void Emit(const std::array<float,N>& value)
{std::cout<<'[';for(std::size_t i=0;i<N;++i){if(i)std::cout<<',';std::cout<<Bits(value[i]);}std::cout<<']';}
void EmitState(const Volume& object,Volume::CameraResultForAnalysis result)
{
    std::cout<<"{\"completed\":"<<result.completed<<",\"nativeReturn\":"<<result.nativeReturn<<",\"cached\":";Emit(object.GetCachedCameraForAnalysis());
    std::cout<<",\"dirty\":"<<object.IsCameraDirtyForAnalysis()<<",\"walkStamp\":"<<object.GetCameraWalkStampForAnalysis()<<",\"faceSides\":[";
    bool comma=false;for(const auto& face:object.GetFacesForAnalysis()){if(comma)std::cout<<',';comma=true;std::cout<<face.cameraSide;}
    std::cout<<"],\"facePlanes\":[";comma=false;for(const auto& plane:object.GetCameraFacePlanesForAnalysis()){if(comma)std::cout<<',';comma=true;Emit(plane);}
    std::cout<<"],\"border\":[";comma=false;for(const auto& point:object.GetBorderPointsForAnalysis()){if(comma)std::cout<<',';comma=true;Emit(point);}
    std::cout<<"],\"edgeStamps\":[";comma=false;for(const auto& edge:object.GetEdgesForAnalysis()){if(comma)std::cout<<',';comma=true;std::cout<<edge->walkStamp;}
    const auto& planes=object.GetOcclusionPlanesForAnalysis();
    std::cout<<"],\"planes\":[";comma=false;for(const auto& plane:planes.planes){if(comma)std::cout<<',';comma=true;std::cout<<'[';for(float value:plane.equation)std::cout<<Bits(value)<<',';std::cout<<plane.enabled<<']';}
    std::cout<<"],\"active\":"<<planes.activeCount<<'}';
}
bool Initialize(Volume& object,const std::vector<Volume::Vector3>& vertices,const std::vector<std::uint16_t>& indices)
{
    spIndexBuffer ib;spVertexBuffer vb;
    if(indices.size()%3||!ib.InitializeForAnalysis(static_cast<std::uint32_t>(indices.size()/3),spIndexBuffer::eIndexBufferType::Type2))return false;
    for(std::uint32_t i=0;i<indices.size();++i)if(!ib.SetIndexForAnalysis(i,indices[i]))return false;
    std::vector<std::byte> bytes(vertices.size()*12);if(!bytes.empty())std::memcpy(bytes.data(),vertices.data(),bytes.size());
    return vb.InitializeFromDataForAnalysis(0,static_cast<std::uint32_t>(vertices.size()),0,bytes)&&
        object.InitializeForAnalysis(ib,vb,sparkplug::host::LegacySortPolicy::GeometryDispatch());
}
void InverseBatch()
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=256,"bounded inverse count");std::cout<<'[';
    for(std::size_t i=0;i<count;++i){const auto position=ReadFloats<3>(),scale=ReadFloats<3>();const auto orientation=ReadFloats<9>();const auto point=ReadFloats<3>();Volume::Vector3 output{};
        const bool complete=math::WorldToLocal420710(point,position,orientation,scale,output);if(i)std::cout<<',';std::cout<<'['<<complete;for(float value:output)std::cout<<','<<Bits(value);std::cout<<']';}
    std::cout<<"]\n";
}
void CameraBatch()
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=32,"bounded camera count");std::cout<<'[';
    for(std::size_t c=0;c<count;++c)
    {
        std::size_t verticesCount,indicesCount,stepCount;Check(bool(std::cin>>verticesCount>>indicesCount>>stepCount)&&verticesCount<=4096&&indicesCount<=3072&&stepCount<=64,"bounded camera input");
        std::vector<Volume::Vector3> vertices(verticesCount);for(auto& vertex:vertices)vertex=ReadFloats<3>();
        std::vector<std::uint16_t> indices(indicesCount);for(auto& index:indices){unsigned value;Check(bool(std::cin>>value)&&value<verticesCount,"camera index input");index=static_cast<std::uint16_t>(value);}
        const auto position=ReadFloats<3>(),scale=ReadFloats<3>();const auto orientation=ReadFloats<9>();
        Volume object;const bool initialized=Initialize(object,vertices,indices);Check(initialized,"camera fresh Init");
        object.SetPositionForAnalysis(position);object.SetScaleForAnalysis(scale);object.SetOrientationForAnalysis(orientation);object.MarkLocalTransformDirtyForAnalysis();Check(object.UpdateWorldForAnalysis(),"camera initial world update");
        if(c)std::cout<<',';std::cout<<"{\"initialized\":"<<initialized<<",\"steps\":[";
        for(std::size_t s=0;s<stepCount;++s){unsigned operation,dirty;Check(bool(std::cin>>operation>>dirty)&&operation<=1&&dirty<=1,"camera step input");const auto camera=ReadFloats<3>();
            if(dirty){object.MarkLocalTransformDirtyForAnalysis();Check(object.UpdateWorldForAnalysis(),"camera forced world update");}
            const auto result=operation?object.BuildCameraPlanesForAnalysis(camera):object.PrepareCameraForAnalysis(camera);if(s)std::cout<<',';EmitState(object,result);}
        std::cout<<"]}";
    }
    std::cout<<"]\n";
}
void Directed()
{
    Volume empty;const auto missing=empty.BuildCameraPlanesForAnalysis({0,0,0});Check(missing.completed&&!missing.nativeReturn,"uninitialized native false");
    Volume object;Check(Initialize(object,{{-2,-2,10},{2,-2,10},{2,2,10},{-2,2,10}},{0,1,2,0,2,3}),"fresh two-triangle Init");
    object.MarkLocalTransformDirtyForAnalysis();Check(object.UpdateWorldForAnalysis(),"world update before camera");
    const auto first=object.BuildCameraPlanesForAnalysis({0,0,0});Check(first.completed&&first.nativeReturn,"fresh camera build");
    Check(!object.IsCameraDirtyForAnalysis()&&object.GetCameraWalkStampForAnalysis()==1,"camera publishes dirty and walk stamp");
    const auto stamp=object.GetCameraWalkStampForAnalysis();const auto cache=object.GetCachedCameraForAnalysis();
    const auto near=object.PrepareCameraForAnalysis({.001f,0,0});Check(near.completed&&near.nativeReturn&&object.GetCameraWalkStampForAnalysis()==stamp&&object.GetCachedCameraForAnalysis()==cache,"inclusive epsilon cache hit retains prior camera/stamp");
    const auto beyond=object.PrepareCameraForAnalysis({std::nextafter(.001f,1.f),0,0});Check(beyond.completed&&beyond.nativeReturn&&object.GetCameraWalkStampForAnalysis()==stamp+1,"one float beyond epsilon rebuilds");
    object.MarkLocalTransformDirtyForAnalysis();Check(object.UpdateWorldForAnalysis(),"explicit dirty world update");const auto forced=object.PrepareCameraForAnalysis(object.GetCachedCameraForAnalysis());Check(forced.completed&&forced.nativeReturn&&object.GetCameraWalkStampForAnalysis()==stamp+2,"dirty overrides same camera cache");
    Volume::Vector3 output{17,18,19};Check(!math::WorldToLocal420710({1,2,3},{0,0,0},{1,0,0,0,1,0,0,0,1},{0,1,1},output)&&output==Volume::Vector3{17,18,19},"zero scale is explicit incomplete domain");
    Check(!math::Near(Volume::Vector3{std::numeric_limits<float>::quiet_NaN(),0,0},cache),"unordered cache comparison is false");
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--inverse-batch"){InverseBatch();return 0;}
    if(argc==2&&std::string(argv[1])=="--camera-batch"){CameraBatch();return 0;}
    Directed();std::cout<<"PASS "<<checks<<"/"<<checks<<": Occlusion camera source\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
