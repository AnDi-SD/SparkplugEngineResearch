// Own shared geometry conversion for bound D3D and current native resources.
// Inputs are borrowed only for this call; no COM, native producer or API calls.
#pragma once
struct SurfaceGeometryInput {
  const std::vector<uint8_t>* vertices=nullptr;
  const std::vector<uint8_t>* indices=nullptr;
  const D3DVERTEXELEMENT9* elements=nullptr;
  UINT elementCount=0,stride=0,offset=0,indexSize=0;
  native_mesh_source::DrawRange range{};
  DWORD cull=0;
  const surface_material::Contract* contract=nullptr;
  const material_channels::Input* channels=nullptr;
};
static bool ExpandSurfaceGeometry(const SurfaceGeometryInput& input,
                                  std::vector<remixapi_HardcodedVertex>& expanded,
                                  material_channels::Plan& plan) {
  expanded.clear();plan={};
  if(!input.vertices||!input.indices||!input.elements||!input.contract||!input.stride||
     !input.elementCount||input.elementCount>MAXD3DDECLLENGTH+1||
     input.elements[input.elementCount-1].Stream!=0xff||
     (input.indexSize!=2&&input.indexSize!=4)||
     (input.range.type!=D3DPT_TRIANGLELIST&&input.range.type!=D3DPT_TRIANGLESTRIP)||
     !input.range.count||input.range.count>32768||!input.range.vertices||input.range.vertices>65536)return false;
  const auto& vertexBytes=*input.vertices;const auto& indexBytes=*input.indices;
  const auto& contract=*input.contract;const auto channels=input.channels;
  const auto layout=input.elements;const UINT n=input.elementCount,stride=input.stride,offset=input.offset,indexSize=input.indexSize;
  const auto type=input.range.type;const INT base=input.range.base;
  const UINT minVertex=input.range.minimum,vertices=input.range.vertices,start=input.range.start,count=input.range.count;
  const DWORD cull=input.cull;
  int pos=-1,normal=-1,color=-1,uv=-1;
  for(UINT i=0;i<n && layout[i].Stream!=0xff;++i) {
    const auto& e=layout[i];if(e.Stream || e.Method!=D3DDECLMETHOD_DEFAULT) return false;
    if(e.Usage==D3DDECLUSAGE_POSITION && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) pos=e.Offset;
    if(e.Usage==D3DDECLUSAGE_NORMAL && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) normal=e.Offset;
    if(e.Usage==D3DDECLUSAGE_COLOR && e.UsageIndex==0 && e.Type==D3DDECLTYPE_D3DCOLOR) color=e.Offset;
    if(e.Usage==D3DDECLUSAGE_TEXCOORD && e.UsageIndex==contract.coordinates && e.Type==D3DDECLTYPE_FLOAT2) uv=e.Offset;
  }
  if(pos<0 || color<0 || uv<0 || UINT(pos+12)>stride || UINT(color+4)>stride || UINT(uv+8)>stride ||
     (channels&&normal<0)||(normal>=0 && UINT(normal+12)>stride)) return false;
  const UINT indexCount=type==D3DPT_TRIANGLELIST?count*3:count+2;
  if(uint64_t(start+uint64_t(indexCount))*indexSize>indexBytes.size()) return false;
  const void* indexData=indexBytes.data()+start*indexSize;
  std::vector<uint32_t> indices(indexCount);
  bool valid=true;
  for(UINT i=0;i<indexCount;++i) {
    const uint32_t original=indexSize==2?static_cast<const uint16_t*>(indexData)[i]:static_cast<const uint32_t*>(indexData)[i];
    const int64_t effective=int64_t(base)+original;
    if(original<minVertex || uint64_t(original)>=uint64_t(minVertex)+vertices || effective<0 ||
       uint64_t(offset)+(uint64_t(effective)+1)*stride>vertexBytes.size()) {valid=false;break;}
    indices[i]=static_cast<uint32_t>(effective);
  }
  if(!valid) return false;
  const void* vertexData=vertexBytes.data();
  expanded.reserve(size_t(count)*3);
  for(UINT tri=0;tri<count;++tri) {
    UINT ids[3]={type==D3DPT_TRIANGLELIST?tri*3:tri,type==D3DPT_TRIANGLELIST?tri*3+1:tri+1,type==D3DPT_TRIANGLELIST?tri*3+2:tri+2};
    if(type==D3DPT_TRIANGLESTRIP && tri%2) std::swap(ids[0],ids[1]);
    if(cull==D3DCULL_CW) std::swap(ids[0],ids[1]);
    remixapi_HardcodedVertex triangle[3]{};
    for(UINT j=0;j<3;++j) {
      auto src=static_cast<const uint8_t*>(vertexData)+offset+size_t(indices[ids[j]])*stride;
      memcpy(triangle[j].position,src+pos,12);memcpy(&triangle[j].color,src+color,4);
      float originalUv[2];memcpy(originalUv,src+uv,8);
      if(!surface_material::Coordinates(contract,originalUv,triangle[j].texcoord)) valid=false;
      if(normal>=0) memcpy(triangle[j].normal,src+normal,12);
      for(float f:triangle[j].position) if(!std::isfinite(f)) valid=false;
      for(float f:triangle[j].texcoord) if(!std::isfinite(f)) valid=false;
      for(float f:triangle[j].normal) if(!std::isfinite(f)) valid=false;
    }
    if(!valid) break;
    if(normal<0) {
      const auto a=triangle[0].position,b=triangle[1].position,c=triangle[2].position;
      const float x=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]);
      const float y=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]);
      const float z=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
      const float length=std::sqrt(x*x+y*y+z*z);if(!std::isfinite(length)){valid=false;break;}if(length<1e-12f) continue;
      for(auto& vtx:triangle) {vtx.normal[0]=x/length;vtx.normal[1]=y/length;vtx.normal[2]=z/length;}
    }
    expanded.insert(expanded.end(),triangle,triangle+3);
  }
  if(!valid || expanded.empty()) return false;
  if(channels) {
    bool uniform=true;const auto firstColor=expanded.front().color;
    for(const auto& v:expanded)if((v.color&0xffffffu)!=(firstColor&0xffffffu)){uniform=false;break;}
    if(!material_channels::Factor(*channels,uniform,firstColor,plan))return false;
    for(auto& v:expanded)v.color=material_channels::Vertex(v.color,plan);
  }
  return true;
}
