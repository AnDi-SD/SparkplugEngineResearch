// Own production signed-skin adapter -> real CPU GameExporter fixture.
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
#include "../winx_skin_packet_gpu.h"

namespace dxvk {
Logger Logger::s_instance("offline-exporter.log",LogLevel::Debug);
static unsigned ownErrorMessages=0;
void messageBox(const char* text,const char* caption,std::uint32_t){
  ++ownErrorMessages;std::fprintf(stderr,"OWN_NONINTERACTIVE_EXPORT_ERROR %s: %s\n",caption,text);
}
}
namespace {
using namespace pxr;
namespace packet=winx_remix::skin_packet;
packet::SignedSkin EncodePose(packet::Packet input,const VtMatrix4dArray& palette) {
  if(palette.size()!=input.palette.size())throw std::runtime_error("Own source palette size");
  for(size_t b=0;b<palette.size();++b)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
    input.palette[b][row][col]=float(palette[b][col][row]);
  packet::SignedSkin encoded;
  if(!packet::EncodeSignedSkin(input,encoded))throw std::runtime_error("Production signed adapter rejected own input");
  return encoded;
}
VtMatrix4dArray UsdPalette(const packet::SignedSkin& encoded) {
  VtMatrix4dArray output(encoded.palette.size(),GfMatrix4d(1));
  for(size_t b=0;b<output.size();++b)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
    output[b][col][row]=encoded.palette[b][row][col];
  return output;
}
void AdaptSigned(lss::Export& data) {
  std::map<uint64_t,packet::Packet> sources;
  std::map<uint64_t,packet::SignedSkin> encodings;
  for(auto& entry:data.meshes) {
    auto& mesh=entry.second;packet::Packet input;input.influences=mesh.bonesPerVertex;
    input.attributes=3;input.palette.resize(mesh.numBones);
    const auto& positions=mesh.buffers.positionBufs.begin()->second;
    const auto& normals=mesh.buffers.normalBufs.begin()->second;
    const auto& weights=mesh.buffers.blendWeightBufs.begin()->second;
    const auto& joints=mesh.buffers.blendIndicesBufs.begin()->second;
    const auto& topology=mesh.buffers.idxBufs.begin()->second;
    input.indices.assign(topology.begin(),topology.end());input.vertices.resize(positions.size());
    for(size_t i=0;i<positions.size();++i) {
      auto& vertex=input.vertices[i];vertex.sourceIndex=uint32_t(i);
      vertex.skin.position[3]=vertex.skin.normal[3]=1;
      for(unsigned a=0;a<3;++a){vertex.skin.position[a]=positions[i][a];vertex.skin.normal[a]=normals[i][a];}
      for(unsigned b=0;b<input.influences;++b){vertex.skin.weights[b]=weights[i*input.influences+b];vertex.skin.indices[b]=joints[i*input.influences+b];}
    }
    const auto encoded=EncodePose(input,mesh.boneXForms);
    sources.emplace(entry.first,input);encodings.emplace(entry.first,encoded);
    mesh.numBones=encoded.palette.size();mesh.bonesPerVertex=encoded.influences;
    lss::Buf<lss::BlendWeight> ws(encoded.weights.size());lss::Buf<lss::BlendIdx> js(encoded.indices.size());
    for(size_t i=0;i<ws.size();++i){ws[i]=encoded.weights[i];js[i]=encoded.indices[i];}
    mesh.buffers.blendWeightBufs.begin()->second=ws;mesh.buffers.blendIndicesBufs.begin()->second=js;
    mesh.boneXForms=UsdPalette(encoded);
  }
  for(auto& entry:data.instances)for(auto& sample:entry.second.boneXForms) {
    const auto encoded=EncodePose(sources.at(entry.second.meshId),sample.xforms);
    const auto& first=encodings.at(entry.second.meshId);
    if(encoded.weights!=first.weights || encoded.indices!=first.indices)throw std::runtime_error("Production adapter changed geometry between poses");
    sample.xforms=UsdPalette(encoded);
  }
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
lss::Export Data(const std::filesystem::path& directory,bool reduce,bool nonuniform,bool defaultJoints,bool singleFrame){
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
    mesh.buffers.idxBufs[0]=lss::Buf<lss::Index>{4,1,5,5,1,2};mesh.buffers.positionBufs[0]=positions;mesh.buffers.normalBufs[0]=normals;
    mesh.buffers.texcoordBufs[0]=uv;mesh.buffers.colorBufs[0]=colors;mesh.buffers.blendWeightBufs[0]=ws;
    if(!defaultJoints)mesh.buffers.blendIndicesBufs[0]=js;
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
  if(singleFrame){
    // Keep pose1, but write one captured frame using USD default-time values.
    e.meta.startTimeCode=e.meta.endTimeCode=0;e.meta.numFramesCaptured=1;
    e.camera.firstTime=e.camera.finalTime=0;e.camera.xforms.resize(1);
    for(auto& entry:e.instances){auto& instance=entry.second;
      auto xform=instance.xforms[instance.xforms.size()==1?0:1];xform.time=0;
      auto palette=instance.boneXForms[instance.boneXForms.size()==1?0:1];palette.time=0;
      instance.firstTime=instance.finalTime=0;instance.xforms={xform};instance.boneXForms={palette};
    }
  }
  return e;
}
}
int main(int argc,char** argv){
  try{
    if(argc<2 || argc>3 || (argc==3 && std::string(argv[2])!="--single-frame"))
      throw std::runtime_error("New output directory and optional --single-frame required");
    const bool singleFrame=argc==3;
    const bool defaultJoints=false;
    const auto root=std::filesystem::absolute(argv[1]);if(std::filesystem::exists(root))throw std::runtime_error("Preserve old output");
    std::filesystem::create_directories(root);
    for(unsigned nonuniform=0;nonuniform<2;++nonuniform)for(unsigned reduce=0;reduce<2;++reduce){
      const auto name=std::string(nonuniform?"nonuniform":"uniform")+(reduce?"-reduced":"-full");
      auto data=Data(root/name,reduce!=0,nonuniform!=0,defaultJoints,singleFrame);
      AdaptSigned(data);
      if(!lss::GameExporter::exportUsd(data))throw std::runtime_error("Actual exporter returned failure");
      std::printf("EXPORTED %s\n",name.c_str());std::fflush(stdout);
    }
    if(dxvk::ownErrorMessages)throw std::runtime_error("Exporter emitted an error dialog request");
    if(GetModuleHandleW(L"vulkan-1.dll")||GetModuleHandleW(L"d3d9.dll"))throw std::runtime_error("Unexpected GPU runtime loaded");
    std::printf("{\"status\":\"EXPORTED_NOT_READ\",\"scenes\":4,\"gpuRuntimeLoaded\":false}\n");return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
