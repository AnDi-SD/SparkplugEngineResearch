#pragma once
// Own explicit API boundary for CPU-baked Skin. Included after the common
// Surface resource owner. It does not classify native shaders/materials, select
// a camera, observe live game pointers, or suppress an original D3D draw.
#include "winx_skin_packet_remix.h"
#include "winx_skin_material_plan.h"
namespace winx_remix::skin_packet_submit {
struct Prepared {
  std::vector<remixapi_HardcodedVertex> vertices;
  std::vector<uint32_t> indices;
  uint32_t sourceAttributes=0;
  DWORD cull=0;
};
// API vertex color is an explicit material-policy input. Fixed COLOR0 can mean
// additive illumination rather than albedo; copying it implicitly is unsafe.
// No reinterpret_cast of BakedVertex and no hidden normalization of weights.
inline bool Prepare(const skin_packet::Packet& source,const std::vector<uint32_t>& materialVertexColors,
                    DWORD cull,Prepared& output,skin_packet::Error* error=nullptr) {
  if(materialVertexColors.size()!=source.vertices.size()||cull<D3DCULL_NONE||cull>D3DCULL_CCW)
    return skin_packet::Fail(error,skin_packet::Error::Layout);
  skin_packet::BakedMesh baked;if(!skin_packet::Bake(source,baked,error))return false;
  try {Prepared result;result.vertices.resize(baked.vertices.size());result.indices=std::move(baked.indices);
    result.sourceAttributes=baked.attributes;result.cull=cull;
    for(size_t i=0;i<baked.vertices.size();++i){const auto& v=baked.vertices[i];auto& out=result.vertices[i];
      memcpy(out.position,v.position.data(),12);memcpy(out.normal,v.normal.data(),12);memcpy(out.texcoord,v.uv.data(),8);
      out.color=materialVertexColors[i]; // Value initialization leaves every API padding word zero.
    }
    if(cull==D3DCULL_CW)for(size_t i=0;i<result.indices.size();i+=3)std::swap(result.indices[i],result.indices[i+1]);
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return skin_packet::Fail(error,skin_packet::Error::Range);}
}
inline remixapi_MeshHandle Resource(const Prepared& input,remixapi_MaterialHandle material) {
  return SurfaceIndexedGeometryResource(input.vertices,input.indices,material);
}
// Connect the explicit initial ColorMode4 policy with the generic geometry
// adapter. The result still needs a material resource, camera and live draw gate.
struct Color4Prepared {Prepared geometry;material_channels::Plan material;};
inline bool PrepareColor4(const skin_packet::Packet& source,const skin_packet::pc::FixedSkinVector& diffuse,
                          DWORD cull,Color4Prepared& output) {
  try {Color4Prepared result;if(!skin_material::ProjectColor4(source,diffuse,result.material))return false;
    std::vector<uint32_t> colors;colors.reserve(source.vertices.size());
    for(const auto& vertex:source.vertices)colors.push_back(material_channels::Vertex(vertex.color,result.material));
    if(!Prepare(source,colors,cull,result.geometry))return false;output=std::move(result);return true;
  }catch(const std::bad_alloc&){return false;}
}
struct DrawResult {
  bool apiCalled=false,apiSucceeded=false,stateStable=false;
};
// Once apiCalled is true, the caller must not replay this same draw as fallback:
// a failure or reentrant retirement cannot prove that no submission occurred.
// Frame/material/owner qualification belongs to the caller before this boundary.
template<class Current> inline DrawResult DrawQualified(remixapi_MeshHandle mesh,remixapi_MaterialHandle material,
    DWORD cull,SurfaceInstanceState state,const surface_material::Contract& contract,unsigned influences,
    const std::vector<remixapi_Transform>* transforms,const Current& current) {
  DrawResult result;SurfaceResourceOperation operation;if(!operation.owned)return result;
  const auto api=GetRemixApi();if(!api||!api->DrawInstance||!mesh||!material||state.cull!=cull||
      cull<D3DCULL_NONE||cull>D3DCULL_CCW)return result;
  if(influences) {
    if(influences>8||!transforms||transforms->empty()||transforms->size()>256)return result;
    for(const auto& matrix:*transforms)for(const auto& row:matrix.matrix)for(float value:row)if(!std::isfinite(value))return result;
  }else if(transforms)return result;
  bool foundMesh=false,foundMaterial=false;
  for(auto& item:surfaceMeshes)if(item.second.handle==mesh&&item.second.material==material&&item.second.usable&&item.second.influences==influences){
    if(influences&&transforms->size()<item.second.requiredBones)return result;
    item.second.frame=frameId;foundMesh=true;break;}
  for(auto& item:surfaceMaterials)if(item.second.handle==material&&item.second.usable){
    item.second.frame=frameId;foundMaterial=true;break;}
  if(!foundMesh||!foundMaterial)return result;
  D3DMATRIX identity{};for(unsigned i=0;i<4;++i)identity.m[i][i]=1;
  remixapi_InstanceInfoBlendEXT blend{};remixapi_InstanceInfo instance{};state.channels=true;
  if(!DescribeSurfaceInstance(state,contract,identity,mesh,blend,instance)||!operation.Stable())return result;
  remixapi_InstanceInfoBoneTransformsEXT bones{};
  if(influences){bones.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;bones.pNext=instance.pNext;
    bones.boneTransforms_values=transforms->data();bones.boneTransforms_count=uint32_t(transforms->size());instance.pNext=&bones;}
  // Revalidate the live caller after every external resource/API lookup and
  // immediately before the single commit. This predicate makes no API calls.
  try {if(!current()||!operation.Stable())return result;}catch(const std::bad_alloc&){return result;}
  result.apiCalled=true;
  try {result.apiSucceeded=api->DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS;}
  catch(const std::bad_alloc&){++surfaceResourceFailures;}
  result.stateStable=operation.Stable();return result;
}
inline DrawResult Draw(remixapi_MeshHandle mesh,remixapi_MaterialHandle material,
                       const Prepared& prepared,SurfaceInstanceState state,const surface_material::Contract& contract) {
  return DrawQualified(mesh,material,prepared.cull,state,contract,0,nullptr,[]{return true;});
}
} // namespace winx_remix::skin_packet_submit
