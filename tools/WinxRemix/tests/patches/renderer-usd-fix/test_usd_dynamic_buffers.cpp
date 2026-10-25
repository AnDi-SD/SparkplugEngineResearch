// Own CPU fixture for sparse attributes, vertex compaction and changing topology.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include "game_exporter.h"
#include "../util/log/log.h"

namespace dxvk {
Logger Logger::s_instance("offline-exporter.log",LogLevel::Debug);
static unsigned ownErrors=0;
void messageBox(const char* text,const char* caption,std::uint32_t){
  ++ownErrors;std::fprintf(stderr,"OWN_EXPORT_ERROR %s: %s\n",caption,text);
}
}
namespace {
using namespace pxr;
enum class Variation { Stable, UsedSet, GrowVertices, ShrinkVertices,
                       ConstantColor, ChangingConstantColor, GrowFaces, ShrinkFaces };
struct Case { const char* name; Variation variation; };
constexpr Case cases[] = {
  {"stable",Variation::Stable}, {"changing",Variation::UsedSet},
  {"growing",Variation::GrowVertices}, {"shrinking",Variation::ShrinkVertices},
  {"constant-stable",Variation::ConstantColor}, {"constant-changing",Variation::ChangingConstantColor},
  {"growing-faces",Variation::GrowFaces}, {"shrinking-faces",Variation::ShrinkFaces}
};
unsigned VertexCount(Variation variation,unsigned time){
  if(variation==Variation::GrowVertices)return time<2?3:6;
  if(variation==Variation::ShrinkVertices)return time<2?6:3;
  return 6;
}
lss::Export Data(const std::filesystem::path& directory,bool reduce,Variation variation){
  const bool changing=variation==Variation::UsedSet || variation==Variation::ChangingConstantColor;
  const bool variableVertices=variation==Variation::GrowVertices || variation==Variation::ShrinkVertices;
  const bool constantColor=variation==Variation::ConstantColor || variation==Variation::ChangingConstantColor;
  std::filesystem::create_directories(directory);
  lss::Export e{};e.debugId="own-dynamic-buffers";e.baseExportPath=directory.string();e.bExportInstanceStage=true;
  e.instanceStagePath=(directory/"own_dynamic.usda").string();e.meta.windowTitle="Own dynamic buffers";
  e.meta.exeName="test_usd_dynamic_buffers.exe";e.meta.iconPath=(directory/"own-icon.txt").string();
  std::ofstream(e.meta.iconPath)<<"own fixture metadata placeholder\n";
  e.meta.geometryHashRule="own-dynamic-buffers";e.meta.metersPerUnit=1;e.meta.timeCodesPerSecond=24;
  e.meta.startTimeCode=0;e.meta.endTimeCode=3;e.meta.numFramesCaptured=4;e.meta.bReduceMeshBuffers=reduce;
  e.camera.fov=1;e.camera.aspectRatio=1;e.camera.nearPlane=.1f;e.camera.farPlane=100;e.camera.firstTime=0;e.camera.finalTime=3;
  for(unsigned t=0;t<4;++t)e.camera.xforms.push_back({double(t),GfMatrix4d(1)});
  lss::Material material{};material.matName="own_dynamic";material.enableOpacity=true;
  material.shaderInputs["inputs:diffuse_color_constant"]=VtValue(GfVec3f(1));e.materials.emplace(100,material);
  lss::Mesh mesh{};mesh.meshName="dynamic";mesh.numVertices=6;mesh.numIndices=3;mesh.matId=100;mesh.isDoubleSided=true;mesh.isLhs=false;
  for(unsigned t: {0u,2u}){
    auto indices=changing ? (t==0?lss::Buf<lss::Index>{0,2,4}:lss::Buf<lss::Index>{1,3,5})
                          : (t==0?lss::Buf<lss::Index>{1,3,5}:lss::Buf<lss::Index>{5,3,1});
    if(variableVertices)indices=t==0?lss::Buf<lss::Index>{0,1,2}:lss::Buf<lss::Index>{2,1,0};
    if((variation==Variation::GrowFaces && t==2) || (variation==Variation::ShrinkFaces && t==0)){
      const auto reverse=lss::Buf<lss::Index>{indices[2],indices[1],indices[0]};
      for(auto index:reverse)indices.push_back(index);
    }
    mesh.buffers.idxBufs[float(t)]=indices;
  }
  mesh.numVertices=VertexCount(variation,0);mesh.numIndices=mesh.buffers.idxBufs.begin()->second.size();
  // Attribute samples deliberately have different time keys from topology.
  const auto positionTimes=variableVertices?std::vector<unsigned>{0,2}:std::vector<unsigned>{0,1,3};
  for(unsigned t:positionTimes){
    const unsigned count=VertexCount(variation,t);
    lss::Buf<lss::Pos> points(count);
    for(unsigned j=0;j<count;++j)points[j]=GfVec3f(float(j)*2+float(t)*.125f,float(j%2)+float(t)*.25f,5+float(j%3)*.125f);
    mesh.buffers.positionBufs[float(t)]=points;
  }
  for(unsigned t: {0u,2u}){
    const unsigned count=VertexCount(variation,t);
    lss::Buf<lss::Norm> normals(count,GfVec3f(0,0,-1));lss::Buf<lss::Texcoord> uv(count);
    lss::Buf<lss::Color> color(constantColor?1:count);
    for(unsigned j=0;j<count;++j)uv[j]=GfVec2f(float(j)*.125f,float(t)*.25f);
    if(constantColor)color[0]=GfVec4f(.25f,.5f+float(t)*.05f,.75f,.625f+float(t)*.05f);
    else for(unsigned j=0;j<count;++j)
      color[j]=GfVec4f(float(j+1)*.1f,float(t)*.05f,.25f,.2f+float(j)*.1f+float(t)*.05f);
    mesh.buffers.normalBufs[float(t)]=normals;mesh.buffers.texcoordBufs[float(t)]=uv;mesh.buffers.colorBufs[float(t)]=color;
  }
  e.meshes.emplace(1,mesh);
  lss::Instance instance{};instance.instanceName="dynamic";instance.meshId=1;instance.matId=100;instance.firstTime=0;instance.finalTime=3;
  for(unsigned t=0;t<4;++t)instance.xforms.push_back({double(t),GfMatrix4d(1)});
  e.instances.emplace(1,instance);
  return e;
}
}
int main(int argc,char** argv){
  try{
    if(argc!=2)throw std::runtime_error("Fresh output directory required");
    const auto output=std::filesystem::absolute(argv[1]);if(std::filesystem::exists(output))throw std::runtime_error("Preserve old output");
    std::filesystem::create_directories(output);
    for(const auto& test:cases)for(bool reduced: {false,true}){
      const auto name=std::string(test.name)+(reduced?"-reduced":"-full");
      auto input=Data(output/name,reduced,test.variation);
      if(!lss::GameExporter::exportUsd(input))throw std::runtime_error("Exporter failed");
      std::printf("EXPORTED %s\n",name.c_str());
    }
    if(dxvk::ownErrors)throw std::runtime_error("Exporter requested error dialog");
    if(GetModuleHandleW(L"vulkan-1.dll")||GetModuleHandleW(L"d3d9.dll"))throw std::runtime_error("Unexpected GPU runtime");
    std::printf("{\"status\":\"EXPORTED_NOT_READ\",\"scenes\":16,\"gpuRuntimeLoaded\":false}\n");return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
