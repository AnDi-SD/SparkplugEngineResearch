#pragma once
// Own immutable mesh / changing palette API representation. Native ownership,
// shader/material meaning and finite deformed normals are qualified by caller.
#include "winx_skin_packet_gpu.h"
namespace winx_remix::skin_gpu_submit {
struct Prepared {
  std::vector<remixapi_HardcodedVertex> vertices;
  std::vector<uint32_t> indices;
  skin_packet::SignedSkin skin;
  std::vector<remixapi_Transform> transforms;
  DWORD cull=0;
};
inline bool Prepare(const skin_packet::Packet& source,const std::vector<uint32_t>& colors,DWORD cull,Prepared& output) {
  if(colors.size()!=source.vertices.size()||cull<D3DCULL_NONE||cull>D3DCULL_CCW)return false;
  try {
    Prepared result;if(!skin_packet::EncodeSignedSkin(source,result.skin))return false;
    result.cull=cull;result.vertices.resize(source.vertices.size());result.indices=source.indices;
    for(size_t i=0;i<source.vertices.size();++i){const auto& input=source.vertices[i];auto& vertex=result.vertices[i];
      memcpy(vertex.position,input.skin.position.data(),12);memcpy(vertex.normal,input.skin.normal.data(),12);
      memcpy(vertex.texcoord,input.uv.data(),8);vertex.color=colors[i];}
    if(cull==D3DCULL_CW)for(size_t i=0;i<result.indices.size();i+=3)std::swap(result.indices[i],result.indices[i+1]);
    result.transforms.resize(result.skin.palette.size());
    for(size_t b=0;b<result.transforms.size();++b)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
      result.transforms[b].matrix[row][col]=result.skin.palette[b][row][col];
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return false;}
}
inline bool Layout(const Prepared& input) {
  const auto b=input.skin.influences;
  if(b<2||b>5||input.cull<D3DCULL_NONE||input.cull>D3DCULL_CCW||input.vertices.empty()||input.vertices.size()>skin_packet::MaximumVertices||
     input.indices.empty()||input.indices.size()>skin_packet::MaximumTriangles*3||input.indices.size()%3||
     input.skin.weights.size()!=input.vertices.size()*b||input.skin.indices.size()!=input.skin.weights.size()||
     input.transforms.size()<3||input.transforms.size()>33||input.transforms.size()%2==0)return false;
  for(uint32_t index:input.indices)if(index>=input.vertices.size())return false;
  for(const auto& vertex:input.vertices) {
    for(float value:vertex.position)if(!std::isfinite(value))return false;
    for(float value:vertex.normal)if(!std::isfinite(value))return false;
    for(float value:vertex.texcoord)if(!std::isfinite(value))return false;
  }
  for(const auto& matrix:input.transforms)for(const auto& row:matrix.matrix)for(float value:row)if(!std::isfinite(value))return false;
  for(size_t i=0;i<input.skin.indices.size();++i)
    if(input.skin.indices[i]>=input.transforms.size()||!std::isfinite(input.skin.weights[i])||input.skin.weights[i]<0)return false;
  return true;
}
// Owner identity separates identical meshes of distinct actors. Palette/pose
// does not enter the immutable key. The owner token must survive revalidation
// and must change on observed native retirement / device reset.
inline remixapi_MeshHandle Resource(const Prepared& input,remixapi_MaterialHandle material,uint64_t ownerIdentity) {
  if(!ownerIdentity||!Layout(input))return nullptr;
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  const uint64_t descriptor[]={0x57585349474e3031ull,ownerIdentity,reinterpret_cast<uintptr_t>(material),input.skin.influences,input.transforms.size()};
  XXH3_state_t state{};XXH3_64bits_reset(&state);XXH3_64bits_update(&state,descriptor,sizeof(descriptor));
  XXH3_64bits_update(&state,input.vertices.data(),input.vertices.size()*sizeof(input.vertices[0]));
  XXH3_64bits_update(&state,input.indices.data(),input.indices.size()*4);
  XXH3_64bits_update(&state,input.skin.weights.data(),input.skin.weights.size()*4);
  XXH3_64bits_update(&state,input.skin.indices.data(),input.skin.indices.size()*4);
  remixapi_MeshInfoSkinning skin{input.skin.influences,input.skin.weights.data(),uint32_t(input.skin.weights.size()),input.skin.indices.data(),uint32_t(input.skin.indices.size())};
  return CreateSurfaceGeometryOwned(input.vertices,input.indices,material,XXH3_64bits_digest(&state),operation,&skin);
}
template<class Current> inline skin_packet_submit::DrawResult Draw(remixapi_MeshHandle mesh,remixapi_MaterialHandle material,
    const Prepared& input,SurfaceInstanceState state,const surface_material::Contract& contract,const Current& current) {
  if(!Layout(input))return {};
  return skin_packet_submit::DrawQualified(mesh,material,input.cull,state,contract,input.skin.influences,&input.transforms,current);
}
} // namespace winx_remix::skin_gpu_submit
