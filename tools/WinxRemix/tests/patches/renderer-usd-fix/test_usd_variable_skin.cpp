// Own CPU fixture. Calls the exact compiled renderer GameExporter library.
// The only substituted host action is a noninteractive error message sink.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "game_exporter.h"
#include "../util/log/log.h"

namespace dxvk {
Logger Logger::s_instance("offline-exporter.log",LogLevel::Debug);
static unsigned ownErrorMessages=0;
void messageBox(const char* text,const char* caption,std::uint32_t){
  ++ownErrorMessages;std::fprintf(stderr,"OWN_NONINTERACTIVE_EXPORT_ERROR %s: %s\n",caption,text);
}
}
namespace {
using namespace pxr;
template<class T> lss::Buf<T> ResizeOwnInput(const lss::Buf<T>& input,size_t count,const T& fill){
  lss::Buf<T> result(count,fill);
  std::copy_n(input.cbegin(),std::min(input.size(),count),result.begin());
  return result;
}
GfMatrix4d Pose(unsigned time,unsigned joint,bool nonuniform){
  if(time==0)return GfMatrix4d(1);
  const unsigned quarter=(time==1?joint:8-joint)%8;
  const double diagonal=std::sqrt(.5);
  const double cosine[]={1,diagonal,0,-diagonal,-1,-diagonal,0,diagonal};
  const double sine[]={0,diagonal,1,diagonal,0,-diagonal,-1,-diagonal};
  const double sx=1+(joint+1)*.125,sy=nonuniform?1+joint*.25:sx,sz=nonuniform?1.5:sx;
  GfMatrix4d m(1);m[0][0]=sx*cosine[quarter];m[0][1]=sx*sine[quarter];
  m[1][0]=-sy*sine[quarter];m[1][1]=sy*cosine[quarter];m[2][2]=sz;
  m[3][0]=time*(joint+1)*.125;m[3][1]=-double(time)*double(joint)*.0625;m[3][2]=time*(joint+1)*.03125;
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
    if(!std::isfinite(m[r][c]) || std::abs(m[r][c])>16)throw std::runtime_error("Own pose out of bounds");
  return m;
}
void Texture(const std::filesystem::path& path,unsigned channel){
  const uint32_t header[32]={0x20534444,124,0x100f,1,1,4,0,1,
    0,0,0,0,0,0,0,0,0,0,0,32,0x41,0,32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000,0x1000,0,0,0,0};
  const uint8_t bgra[4]={uint8_t(96+4*channel),uint8_t(64+8*channel),uint8_t(32+16*channel),192};
  std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(header),sizeof(header));f.write(reinterpret_cast<const char*>(bgra),sizeof(bgra));
  if(!f)throw std::runtime_error("Own DDS write failed");
}
lss::Export Data(const std::filesystem::path& directory,bool reduce,bool nonuniform,bool defaultJoints){
  std::filesystem::create_directories(directory/"textures");
  lss::Export e{};e.debugId="own-offline";e.baseExportPath=directory.string();
  e.bExportInstanceStage=true;e.instanceStagePath=(directory/"own_offline.usda").string();
  e.meta.windowTitle="Own offline USD fixture";e.meta.exeName="test_usd_exporter.exe";
  e.meta.iconPath=(directory/"own-icon.txt").string();std::ofstream(e.meta.iconPath)<<"own fixture icon placeholder; not a game asset\n";
  e.meta.geometryHashRule="own-explicit-ids";e.meta.metersPerUnit=1;e.meta.timeCodesPerSecond=24;
  e.meta.startTimeCode=0;e.meta.endTimeCode=2;e.meta.numFramesCaptured=3;e.meta.bReduceMeshBuffers=reduce;
  e.camera.fov=1;e.camera.aspectRatio=16.f/9;e.camera.nearPlane=.1f;e.camera.farPlane=100;
  e.camera.firstTime=0;e.camera.finalTime=2;
  for(unsigned t=0;t<3;++t)e.camera.xforms.push_back({double(t),GfMatrix4d(1)});
  lss::Material material{};material.matName="own_opaque";material.enableOpacity=true;
  auto& a=material.shaderInputs;
  a["inputs:anisotropy"]=VtValue(.3f);a["inputs:diffuse_color_constant"]=VtValue(GfVec3f(.2f,.4f,.6f));
  a["inputs:opacity_constant"]=VtValue(.75f);a["inputs:reflection_roughness_constant"]=VtValue(.35f);a["inputs:metallic_constant"]=VtValue(.6f);
  a["inputs:emissive_intensity"]=VtValue(2.f);a["inputs:emissive_color_constant"]=VtValue(GfVec3f(.1f,.3f,.5f));a["inputs:enable_emission"]=VtValue(true);
  a["inputs:sprite_sheet_rows"]=VtValue(3u);a["inputs:sprite_sheet_cols"]=VtValue(4u);a["inputs:sprite_sheet_fps"]=VtValue(9u);
  a["inputs:enable_thin_film"]=VtValue(true);a["inputs:thin_film_thickness_constant"]=VtValue(320.f);a["inputs:thin_film_thickness_from_albedo_alpha"]=VtValue(false);
  a["inputs:use_legacy_alpha_state"]=VtValue(true);a["inputs:blend_enabled"]=VtValue(true);a["inputs:blend_type"]=VtValue(0u);a["inputs:inverted_blend"]=VtValue(true);
  a["inputs:alpha_test_type"]=VtValue(7u);a["inputs:alpha_test_reference_value"]=VtValue(97u);
  a["inputs:displace_in"]=VtValue(.04f);a["inputs:displace_out"]=VtValue(.02f);
  a["inputs:subsurface_transmittance_color"]=VtValue(GfVec3f(.2f,.5f,.7f));a["inputs:subsurface_measurement_distance"]=VtValue(.6f);
  a["inputs:subsurface_single_scattering_albedo"]=VtValue(GfVec3f(.3f,.4f,.8f));a["inputs:subsurface_volumetric_anisotropy"]=VtValue(-.2f);
  a["inputs:subsurface_diffusion_profile"]=VtValue(true);a["inputs:subsurface_radius"]=VtValue(GfVec3f(.1f,.2f,.4f));
  a["inputs:subsurface_radius_scale"]=VtValue(.75f);a["inputs:subsurface_max_sample_radius"]=VtValue(1.5f);
  a["inputs:filter_mode"]=VtValue(1u);a["inputs:wrap_mode_u"]=VtValue(1u);a["inputs:wrap_mode_v"]=VtValue(2u);
  const char* channels[]={"diffuse_texture","normalmap_texture","tangent_texture","height_texture","reflectionroughness_texture","metallic_texture",
    "emissive_mask_texture","subsurface_transmittance_texture","subsurface_thickness_texture","subsurface_single_scattering_texture","subsurface_radius_texture","secondary_texture"};
  for(unsigned c=0;c<12;++c){const auto path=directory/"textures"/("own-channel-"+std::to_string(c)+".dds");Texture(path,c);material.textureInputs["inputs:"+std::string(channels[c])]=path.string();}
  e.materials.emplace(100,std::move(material));
  const float weights[4][16]={
    {.5f,1.f,1.25f,.75f},{-.25f,1.25f,.75f,.25f,1.5f,-.5f,0.f,1.f},
    {.25f,.5f,.25f,-.25f,.75f,.5f,.125f,.25f,.25f,0.f,0.f,1.25f},
    {.125f,.25f,.375f,.25f,-.5f,.25f,.75f,.5f,.5f,.25f,.25f,.25f,0.f,0.f,0.f,.75f}};
  const unsigned slots[]={4,1,5,2};
  for(unsigned c=0;c<4;++c){
    const unsigned n=c+1;lss::Mesh mesh{};mesh.meshName="case"+std::to_string(n);mesh.numVertices=6;mesh.numIndices=6;
    mesh.matId=100;mesh.isDoubleSided=true;mesh.numBones=4;mesh.bonesPerVertex=n;mesh.isLhs=false;
    lss::Buf<lss::Pos> positions(6,GfVec3f(99,99,99));lss::Buf<lss::Norm> normals(6,GfVec3f(0,0,-1));
    lss::Buf<lss::Texcoord> uv(6,GfVec2f(0));lss::Buf<lss::Color> colors(6,GfVec4f(.25f,.5f,.75f,.625f));
    lss::Buf<lss::BlendWeight> ws(6*n,0);lss::Buf<lss::BlendIdx> js(6*n,0);
    for(unsigned i=0;i<4;++i){const unsigned slot=slots[i];positions[slot]=GfVec3f(1.25f+(i&1?.5f:-.5f),(float(c)-1.5f)*.75f+(i&2?-.25f:.25f),5);
      uv[slot]=GfVec2f(float(i&1),float(i>>1));for(unsigned k=0;k<n;++k){ws[slot*n+k]=weights[c][i*n+k];js[slot*n+k]=(i+k)%4;}}
    // Own variable-array fixture: uniform grows6->8, nonuniform shrinks8->6.
    mesh.numVertices=nonuniform?8:6;
    mesh.buffers.idxBufs[0]=lss::Buf<lss::Index>{4,1,5,5,1,2};
    for(unsigned t: {0u,2u}){
      const unsigned count=((t==0)==nonuniform)?8u:6u;
      auto p=ResizeOwnInput(positions,count,GfVec3f(99));
      for(auto slot:slots){p[slot][0]+=float(t)*.125f;p[slot][1]+=float(t)*.25f;}
      auto ns=ResizeOwnInput(normals,count,GfVec3f(0,0,-1));
      auto u=ResizeOwnInput(uv,count,GfVec2f(0));
      auto cs=ResizeOwnInput(colors,count,GfVec4f(.25f,.5f,.75f,.625f));
      for(auto& color:cs){color[0]+=float(t)*.05f;color[3]+=float(t)*.025f;}
      auto w=ResizeOwnInput(ws,count*n,lss::BlendWeight(0));for(auto& weight:w)weight*=1+float(t)*.125f;
      auto j=ResizeOwnInput(js,count*n,lss::BlendIdx(0));
      mesh.buffers.positionBufs[float(t)]=p;mesh.buffers.normalBufs[float(t)]=ns;
      mesh.buffers.texcoordBufs[float(t)]=u;mesh.buffers.colorBufs[float(t)]=cs;mesh.buffers.blendWeightBufs[float(t)]=w;
      if(!defaultJoints)mesh.buffers.blendIndicesBufs[float(t)]=j;
    }
    mesh.boneXForms=VtMatrix4dArray(4,GfMatrix4d(1));e.meshes.emplace(c,mesh);
    for(unsigned late=0;late<2;++late){lss::Instance instance{};instance.instanceName="case"+std::to_string(n)+(late?"_late":"_full");
      instance.meshId=c;instance.matId=100;instance.firstTime=late?1:0;instance.finalTime=late?1:2;
      for(unsigned t=unsigned(instance.firstTime);t<=unsigned(instance.finalTime);++t){
        auto transform=GfMatrix4d(1);transform.SetTranslate(GfVec3d(.125*t,.25*late,0));instance.xforms.push_back({double(t),transform});
        VtMatrix4dArray palette(4);for(unsigned j=0;j<4;++j)palette[j]=Pose(t,j,nonuniform);
        instance.boneXForms.push_back({double(t),palette});
      }
      e.instances.emplace(c*2+late,std::move(instance));
    }
  }
  return e;
}
}
int main(int argc,char** argv){
  try{
    if(argc<2 || argc>3 || (argc==3 && std::string(argv[2])!="--default-joints"))
      throw std::runtime_error("New output directory and optional --default-joints required");
    const bool defaultJoints=argc==3;
    const auto root=std::filesystem::absolute(argv[1]);if(std::filesystem::exists(root))throw std::runtime_error("Preserve old output");
    std::filesystem::create_directories(root);
    for(unsigned nonuniform=0;nonuniform<2;++nonuniform)for(unsigned reduce=0;reduce<2;++reduce){
      const auto name=std::string(nonuniform?"nonuniform":"uniform")+(reduce?"-reduced":"-full");
      auto data=Data(root/name,reduce!=0,nonuniform!=0,defaultJoints);
      if(!lss::GameExporter::exportUsd(data))throw std::runtime_error("Actual exporter returned failure");
      std::printf("EXPORTED %s\n",name.c_str());std::fflush(stdout);
    }
    if(dxvk::ownErrorMessages)throw std::runtime_error("Exporter emitted an error dialog request");
    if(GetModuleHandleW(L"vulkan-1.dll")||GetModuleHandleW(L"d3d9.dll"))throw std::runtime_error("Unexpected GPU runtime loaded");
    std::printf("{\"status\":\"EXPORTED_NOT_READ\",\"scenes\":4,\"gpuRuntimeLoaded\":false}\n");return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
