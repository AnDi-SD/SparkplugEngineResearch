// Own bounded translation of a recognized FFP overlay into explicit Remix API
// resources, including separate FFP color coefficients. No asset IDs or game logic.
#include "winx_surface_material.h"
#include "winx_material_channels.h"
#include "winx_material_channel_assets.h"
#include "winx_geometry_probe.h"
#include <algorithm>
static bool materialChannelsEnabled,keepMaterialChannelsForComparison;
static bool preserveUnlitColor=true;
static unsigned materialChannelsSubmitted,materialChannelsRejected;
static wchar_t surfaceAssetDirectory[MAX_PATH]{};
struct SurfaceMesh {
  remixapi_MeshHandle handle;unsigned frame;size_t bytes;remixapi_MaterialHandle material;
  unsigned failedPressureFrame=0;bool pressureFailed=false;
};
struct SurfaceMaterialEntry {
  remixapi_MaterialHandle handle;unsigned frame;
  unsigned failedPressureFrame=0;bool pressureFailed=false;
};
static std::map<uint64_t,SurfaceMesh> surfaceMeshes;
static std::map<uint64_t,SurfaceMaterialEntry> surfaceMaterials;
static size_t surfaceMeshBytes;
static unsigned surfaceMeshCreates,surfaceMeshDestroys,surfaceMaterialCreates,surfaceMaterialDestroys,surfaceResourceFailures;
static unsigned surfacePressureRequests,surfacePressureMeshDestroys,surfacePressureMaterialDestroys,surfacePressureRejected;
static constexpr size_t surfaceMeshLimit=512,surfaceMaterialLimit=256,surfaceMeshByteLimit=64*1024*1024;
struct SurfaceBuffer { std::vector<uint8_t> bytes; bool complete=false; };
struct SurfaceWrite { const void* data; UINT offset,size,total; DWORD flags; };
static std::map<void*,SurfaceBuffer> surfaceBuffers;
static std::map<void*,SurfaceWrite> surfaceWrites;
static size_t surfaceBufferBytes;
static void ForgetSurfaceBuffer(void* b) {
  auto it=surfaceBuffers.find(b);
  if(it!=surfaceBuffers.end()) {surfaceBufferBytes-=it->second.bytes.size();surfaceBuffers.erase(it);}
  surfaceWrites.erase(b);
}
static void CaptureSurfaceWrite(void* b) {
  auto it=surfaceWrites.find(b);if(it==surfaceWrites.end()) return;
  const auto w=it->second;surfaceWrites.erase(it);
  if(!w.total || w.total>8*1024*1024 || w.offset>w.total || w.size>w.total-w.offset) {ForgetSurfaceBuffer(b);return;}
  auto found=surfaceBuffers.find(b);
  if(found==surfaceBuffers.end()) {
    if(surfaceBuffers.size()>=4096 || surfaceBufferBytes+w.total>64*1024*1024) return;
    found=surfaceBuffers.emplace(b,SurfaceBuffer{std::vector<uint8_t>(w.total),false}).first;surfaceBufferBytes+=w.total;
  }
  auto& snapshot=found->second;
  if(snapshot.bytes.size()!=w.total) {ForgetSurfaceBuffer(b);return;}
  if(w.flags&D3DLOCK_DISCARD) snapshot.complete=false;
  memcpy(snapshot.bytes.data()+w.offset,w.data,w.size);
  if(w.offset==0 && w.size==w.total) snapshot.complete=true;
}

// Observation also covers FFP positions rejected by the material UV/normal
// contract. These draws still reach stock Remix and can occlude its lights.
static void RecordFfpGeometry(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minVertex,UINT vertices,UINT start,UINT count) {
  if(!GeometryProbeRequested())return;
  IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
  struct Release {IDirect3DVertexBuffer9*& v;IDirect3DIndexBuffer9*& i;IDirect3DVertexDeclaration9*& d;~Release(){if(v)v->Release();if(i)i->Release();if(d)d->Release();}} release{vb,ib,decl};
  UINT offset=0,stride=0,n=MAXD3DDECLLENGTH+1;D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH+1]{};
  D3DINDEXBUFFER_DESC id{};D3DMATRIX world{};DWORD cull=0;
  if(count>32768||vertices>65536||FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||!vb||!stride||
     FAILED(d->GetIndices(&ib))||!ib||FAILED(ib->GetDesc(&id))||(id.Format!=D3DFMT_INDEX16&&id.Format!=D3DFMT_INDEX32)||
     FAILED(d->GetVertexDeclaration(&decl))||!decl||FAILED(decl->GetDeclaration(layout,&n))||
     FAILED(d->GetTransform(D3DTS_WORLD,&world))||FAILED(d->GetRenderState(D3DRS_CULLMODE,&cull)))return;
  int position=-1;for(UINT i=0;i<n&&layout[i].Stream!=0xff;++i){const auto& e=layout[i];if(e.Stream)return;
    if(e.Usage==D3DDECLUSAGE_POSITION&&e.UsageIndex==0&&e.Type==D3DDECLTYPE_FLOAT3)position=e.Offset;}
  if(position<0||UINT(position+12)>stride)return;
  const auto v=surfaceBuffers.find(vb),ind=surfaceBuffers.find(ib);
  if(v==surfaceBuffers.end()||ind==surfaceBuffers.end()||!v->second.complete||!ind->second.complete)return;
  const UINT indexSize=id.Format==D3DFMT_INDEX16?2:4,indexCount=type==D3DPT_TRIANGLELIST?count*3:count+2;
  if((uint64_t(start)+indexCount)*indexSize>ind->second.bytes.size())return;
  std::vector<remixapi_HardcodedVertex> expanded;expanded.reserve(size_t(count)*3);
  for(UINT tri=0;tri<count;++tri)for(UINT j=0;j<3;++j){
    const UINT corner=type==D3DPT_TRIANGLESTRIP&&(tri&1)&&j<2?1-j:j;
    const UINT index=start+(type==D3DPT_TRIANGLELIST?tri*3:tri)+corner;
    uint32_t value=0;memcpy(&value,ind->second.bytes.data()+size_t(index)*indexSize,indexSize);
    const auto effective=int64_t(base)+value;
    if(value<minVertex||uint64_t(value)>=uint64_t(minVertex)+vertices||effective<0||uint64_t(offset)+(uint64_t(effective)+1)*stride>v->second.bytes.size())return;
    remixapi_HardcodedVertex vertex{};memcpy(vertex.position,v->second.bytes.data()+offset+size_t(effective)*stride+position,12);expanded.push_back(vertex);}
  const auto hash=XXH3_64bits(expanded.data(),expanded.size()*sizeof(expanded[0]));RecordGeometryProbe(expanded,world,hash,cull);
}

static void RetireSurfaceResources(bool all=false) {
  if((!all&&frameId%30) || (surfaceMeshes.empty() && surfaceMaterials.empty())) return;
  auto api=GetRemixApi();if(!api || !api->DestroyMesh || !api->DestroyMaterial) return;
  for(auto it=surfaceMeshes.begin();it!=surfaceMeshes.end();) {
    if(all)it->second.frame=frameId-301u;
    if(frameId-it->second.frame>300) {
      if(api->DestroyMesh(it->second.handle)!=REMIXAPI_ERROR_CODE_SUCCESS) {++surfaceResourceFailures;++it;continue;}
      ++surfaceMeshDestroys;surfaceMeshBytes-=it->second.bytes;
      it=surfaceMeshes.erase(it);
    } else ++it;
  }
  // Failed mesh deletions retain their material reference. Never free an API
  // material while any owned mesh can still use it, including expired retries.
  std::set<remixapi_MaterialHandle> referenced;
  for(const auto& item:surfaceMeshes)referenced.insert(item.second.material);
  for(auto it=surfaceMaterials.begin();it!=surfaceMaterials.end();) {
    if(all)it->second.frame=frameId-301u;
    if(frameId-it->second.frame>300 && !referenced.count(it->second.handle)) {
      if(api->DestroyMaterial(it->second.handle)!=REMIXAPI_ERROR_CODE_SUCCESS) {++surfaceResourceFailures;++it;continue;}
      ++surfaceMaterialDestroys;it=surfaceMaterials.erase(it);
    } else ++it;
  }
}
// Own backend pressure policy. All candidates are previous-frame resources;
// unsigned age ordering also works across frame-counter wrap. An API failure
// preserves ownership and is attempted at most once per resource per frame by
// this policy. TTL/forced retirement retains its independent retry behavior.
static bool EnsureSurfaceResourceRoom(size_t meshes,size_t bytes,size_t materials,
                                      remixapi_MaterialHandle protectedMaterial=nullptr) {
  const auto room=[&]() {
    return meshes<=surfaceMeshLimit&&materials<=surfaceMaterialLimit&&bytes<=surfaceMeshByteLimit&&
      surfaceMeshes.size()<=surfaceMeshLimit-meshes&&surfaceMaterials.size()<=surfaceMaterialLimit-materials&&
      surfaceMeshBytes<=surfaceMeshByteLimit-bytes;
  };
  if(room())return true;
  ++surfacePressureRequests;
  if(meshes>surfaceMeshLimit||materials>surfaceMaterialLimit||bytes>surfaceMeshByteLimit) {
    ++surfacePressureRejected;return false;
  }
  auto api=GetRemixApi();
  if(!api||!api->DestroyMesh||!api->DestroyMaterial) {++surfacePressureRejected;return false;}
  const auto orphans=[&]() {
    for(auto it=surfaceMaterials.begin();it!=surfaceMaterials.end();) {
      auto& value=it->second;
      if(value.frame==frameId||value.handle==protectedMaterial||
         (value.pressureFailed&&value.failedPressureFrame==frameId)) {++it;continue;}
      bool referenced=false;
      for(const auto& mesh:surfaceMeshes)if(mesh.second.material==value.handle){referenced=true;break;}
      if(referenced){++it;continue;}
      if(api->DestroyMaterial(value.handle)!=REMIXAPI_ERROR_CODE_SUCCESS) {
        value.pressureFailed=true;value.failedPressureFrame=frameId;++surfaceResourceFailures;++it;continue;
      }
      ++surfaceMaterialDestroys;++surfacePressureMaterialDestroys;it=surfaceMaterials.erase(it);
      if(room())break;
    }
  };
  orphans();
  if(room())return true;
  // Fixed scratch storage and one sorted traversal: no pressure-time heap
  // allocation or unbounded retry loop. At most 512 mesh/256 material attempts.
  struct Candidate {uint64_t key;unsigned age;};
  Candidate candidates[surfaceMeshLimit]{};size_t count=0;
  for(const auto& item:surfaceMeshes) {
    const auto& value=item.second;
    if(value.frame!=frameId&&!(value.pressureFailed&&value.failedPressureFrame==frameId)&&count<surfaceMeshLimit)
      candidates[count++]={item.first,frameId-value.frame};
  }
  std::sort(candidates,candidates+count,[](const Candidate& a,const Candidate& b){
    return a.age!=b.age?a.age>b.age:a.key<b.key;
  });
  for(size_t index=0;index<count&&!room();++index) {
    auto it=surfaceMeshes.find(candidates[index].key);if(it==surfaceMeshes.end())continue;
    auto& value=it->second;
    if(surfaceMeshes.size()<=surfaceMeshLimit-meshes&&surfaceMeshBytes<=surfaceMeshByteLimit-bytes) {
      // Only a material slot is missing. Evicting meshes cannot help if their
      // material is protected, failed deletion, or has a current-frame user.
      bool eligible=false;
      for(const auto& entry:surfaceMaterials)if(entry.second.handle==value.material) {
        const auto& material=entry.second;
        eligible=material.frame!=frameId&&material.handle!=protectedMaterial&&
          !(material.pressureFailed&&material.failedPressureFrame==frameId);break;
      }
      if(eligible)for(const auto& mesh:surfaceMeshes)
        if(mesh.second.material==value.material&&mesh.second.frame==frameId){eligible=false;break;}
      if(!eligible)continue;
    }
    if(api->DestroyMesh(value.handle)!=REMIXAPI_ERROR_CODE_SUCCESS) {
      value.pressureFailed=true;value.failedPressureFrame=frameId;++surfaceResourceFailures;continue;
    }
    ++surfaceMeshDestroys;++surfacePressureMeshDestroys;surfaceMeshBytes-=value.bytes;surfaceMeshes.erase(it);
    orphans();
  }
  const bool result=room();if(!result)++surfacePressureRejected;return result;
}
static void SetPreserveUnlitColor(bool value) {
  if(value==preserveUnlitColor)return;
  // A policy switch invalidates every material coefficient. Retire old meshes
  // before their materials so the two variants cannot exhaust the bounded
  // cache and force a partially translated scene during live comparison.
  RetireSurfaceResources(true);preserveUnlitColor=value;
}

static remixapi_MaterialHandle SurfaceMaterial(IDirect3DDevice9* d,uint64_t textureHash) {
  DWORD u=0,v=0,mag=0;
  if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u)) || FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v)) ||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag)) || u<1 || u>3 || v<1 || v>3 || mag<1 || mag>3) return nullptr;
  const uint64_t descriptor[]={textureHash,u,v,mag};
  const auto hash=XXH3_64bits(descriptor,sizeof(descriptor));
  auto found=surfaceMaterials.find(hash);if(found!=surfaceMaterials.end()) {found->second.frame=frameId;return found->second.handle;}
  if(!surfaceAssetDirectory[0]) return nullptr;
  using SaveTexture=HRESULT(WINAPI*)(LPCWSTR,int,IDirect3DBaseTexture9*,const PALETTEENTRY*);
  static auto save=[](){auto dll=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    return dll?reinterpret_cast<SaveTexture>(GetProcAddress(dll,"D3DXSaveTextureToFileW")):nullptr;}();
  if(!save) return nullptr;
  wchar_t path[MAX_PATH]{};
  swprintf_s(path,L"%s\\%016llX.dds",surfaceAssetDirectory,static_cast<unsigned long long>(textureHash));
  if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES) {
    IDirect3DBaseTexture9* texture=nullptr;if(FAILED(d->GetTexture(0,&texture)) || !texture) return nullptr;
    const auto hr=save(path,4,texture,nullptr);texture->Release();if(FAILED(hr)) return nullptr;
  }
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=0.5f;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;
  info.hash=hash;info.albedoTexture=path;
  // API uses Vulkan sampler enums: repeat=0, mirrored repeat=1, clamp=2.
  info.wrapModeU=static_cast<uint8_t>(u-1);info.wrapModeV=static_cast<uint8_t>(v-1);
  info.filterMode=mag==D3DTEXF_POINT?0:1;
  remixapi_MaterialHandle material=nullptr;
  if(!EnsureSurfaceResourceRoom(0,0,1))return nullptr;
  if(GetRemixApi()->CreateMaterial(&info,&material)!=REMIXAPI_ERROR_CODE_SUCCESS || !material) return nullptr;
  surfaceMaterials.emplace(hash,SurfaceMaterialEntry{material,frameId});++surfaceMaterialCreates;return material;
}

static remixapi_MaterialHandle SurfaceChannelMaterial(IDirect3DDevice9* d,uint64_t textureHash,const material_channels::Plan& plan) {
  DWORD u=0,v=0,mag=0,srgb=0;
  if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u))||FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v))||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag))||FAILED(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb))||srgb||
     u<1||u>3||v<1||v>3||mag<1||mag>3||!surfaceAssetDirectory[0])return nullptr;
  std::string descriptor="winx-independent-ffp-v1";
  const uint64_t values[]={textureHash,u,v,mag};descriptor.append(reinterpret_cast<const char*>(values),sizeof(values));
  descriptor.append(reinterpret_cast<const char*>(&plan.albedo),sizeof(plan.albedo));
  descriptor.append(reinterpret_cast<const char*>(&plan.emission),sizeof(plan.emission));
  const auto hash=XXH3_64bits(descriptor.data(),descriptor.size());
  auto found=surfaceMaterials.find(hash);if(found!=surfaceMaterials.end()){found->second.frame=frameId;return found->second.handle;}
  wchar_t albedo[MAX_PATH]{},emission[MAX_PATH]{};
  swprintf_s(albedo,L"%s\\%016llX-albedo.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  swprintf_s(emission,L"%s\\%016llX-emission.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  const bool emissive=!material_channels::Zero(plan.emission);
  if(!material_channels::WriteTexture(d,albedo,plan.albedo)||
     (emissive&&!material_channels::WriteTexture(d,emission,plan.emission)))return nullptr;
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=.5f;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;info.hash=hash;
  info.albedoTexture=albedo;info.emissiveTexture=emissive?emission:nullptr;info.emissiveIntensity=emissive?1.f:0.f;
  info.wrapModeU=static_cast<uint8_t>(u-1);info.wrapModeV=static_cast<uint8_t>(v-1);info.filterMode=mag==D3DTEXF_POINT?0:1;
  remixapi_MaterialHandle material=nullptr;
  if(!EnsureSurfaceResourceRoom(0,0,1))return nullptr;
  if(GetRemixApi()->CreateMaterial(&info,&material)!=REMIXAPI_ERROR_CODE_SUCCESS||!material)return nullptr;
  surfaceMaterials.emplace(hash,SurfaceMaterialEntry{material,frameId});++surfaceMaterialCreates;return material;
}

static bool SurfaceSubmitFailure(unsigned line) {
  static std::set<unsigned> reported;
  if(surfaceRoleLog && reported.insert(line).second) {
    fprintf(surfaceRoleLog,"{\"event\":\"submit_unsupported\",\"frame\":%u,\"line\":%u}\n",frameId,line);fflush(surfaceRoleLog);
  }
  return false;
}

static bool SubmitSurfaceOverlay(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minVertex,UINT vertices,
                                 UINT start,UINT count,uint64_t textureHash,const material_channels::Input* channels=nullptr) {
  auto api=GetRemixApi();
  if(!api || !api->CreateMesh || !api->DrawInstance || !api->CreateMaterial || count>32768 || vertices>65536) return SurfaceSubmitFailure(__LINE__);
  surface_material::Contract contract;
  DWORD cull=0,separateAlpha=0,stencil=0;
  if(!(channels?surface_material::ReadTexture(d,contract):surface_material::Read(d,contract))||
     (channels&&!material_channels::Texture(contract,contract))||FAILED(d->GetRenderState(D3DRS_CULLMODE,&cull))||
     FAILED(d->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&separateAlpha))||separateAlpha||
     FAILED(d->GetRenderState(D3DRS_STENCILENABLE,&stencil))||stencil) return SurfaceSubmitFailure(__LINE__);
  IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
  struct Release { IDirect3DVertexBuffer9*& v;IDirect3DIndexBuffer9*& i;IDirect3DVertexDeclaration9*& d;
    ~Release(){if(v)v->Release();if(i)i->Release();if(d)d->Release();} } release{vb,ib,decl};
  UINT offset=0,stride=0,n=MAXD3DDECLLENGTH+1;D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH+1]{};
  if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride)) || !vb || !stride || FAILED(d->GetIndices(&ib)) || !ib ||
     FAILED(d->GetVertexDeclaration(&decl)) || !decl || FAILED(decl->GetDeclaration(layout,&n))) return SurfaceSubmitFailure(__LINE__);
  D3DVERTEXBUFFER_DESC vd{};D3DINDEXBUFFER_DESC id{};
  if(FAILED(vb->GetDesc(&vd)) || FAILED(ib->GetDesc(&id)) ||
     (id.Format!=D3DFMT_INDEX16 && id.Format!=D3DFMT_INDEX32)) return SurfaceSubmitFailure(__LINE__);
  const auto vbSnapshot=surfaceBuffers.find(vb),ibSnapshot=surfaceBuffers.find(ib);
  if(vbSnapshot==surfaceBuffers.end() || ibSnapshot==surfaceBuffers.end() || !vbSnapshot->second.complete ||
     !ibSnapshot->second.complete || vbSnapshot->second.bytes.size()!=vd.Size || ibSnapshot->second.bytes.size()!=id.Size) return SurfaceSubmitFailure(__LINE__);
  // One converter/backend for both sources. The native packet replaces raw
  // bytes, range, world and recovered vertex layout. Material/render states
  // remain D3D inputs. No durable native instance identity is inferred here.
  native_mesh_source::Geometry nativeGeometry{};
  bool nativeInput=native_mesh_source::Resolve(d,{type,base,minVertex,vertices,start,count},vb,ib,offset,stride,nativeGeometry);
  if(nativeInput&&(!nativeGeometry.vertices->verified||!nativeGeometry.indices->verified)) {
    const bool equal=native_mesh_source::EqualsUpload(*nativeGeometry.vertices,vbSnapshot->second.bytes)&&
      native_mesh_source::EqualsUpload(*nativeGeometry.indices,ibSnapshot->second.bytes);
    if(native_mesh_source::output&&_ftelli64(native_mesh_source::output)<16*1024*1024)
      fprintf(native_mesh_source::output,"{\"event\":\"compare_upload\",\"frame\":%u,\"generation\":%llu,\"equal\":%s}\n",frameId,nativeGeometry.vertices->generation,equal?"true":"false");
    if(equal){nativeGeometry.vertices->verified=true;nativeGeometry.indices->verified=true;}
    else{nativeInput=false;++native_mesh_source::uploadMismatches;}
  }
  // The native declaration cache can be reinitialized without changing its
  // flags key. Preserve that original behavior by comparing on every draw.
  if(nativeInput&&!native_mesh_source::EqualLayout(nativeGeometry,layout,n)) {
    nativeInput=false;++native_mesh_source::layoutMismatches;
  }
  nativeInput=nativeInput&&native_mesh_source::submitEnabled;
  const auto& vertexBytes=nativeInput?nativeGeometry.vertices->data:vbSnapshot->second.bytes;
  const auto& indexBytes=nativeInput?nativeGeometry.indices->data:ibSnapshot->second.bytes;
  if(nativeInput){const auto& range=nativeGeometry.range;type=range.type;base=range.base;minVertex=range.minimum;
    vertices=range.vertices;start=range.start;count=range.count;stride=nativeGeometry.stride;}
  if(nativeInput) {
    n=UINT(nativeGeometry.layout->size());
    memcpy(layout,nativeGeometry.layout->data(),size_t(n)*sizeof(layout[0]));
  }
  int pos=-1,normal=-1,color=-1,uv=-1;
  for(UINT i=0;i<n && layout[i].Stream!=0xff;++i) {
    const auto& e=layout[i];if(e.Stream || e.Method!=D3DDECLMETHOD_DEFAULT) return SurfaceSubmitFailure(__LINE__);
    if(e.Usage==D3DDECLUSAGE_POSITION && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) pos=e.Offset;
    if(e.Usage==D3DDECLUSAGE_NORMAL && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) normal=e.Offset;
    if(e.Usage==D3DDECLUSAGE_COLOR && e.UsageIndex==0 && e.Type==D3DDECLTYPE_D3DCOLOR) color=e.Offset;
    if(e.Usage==D3DDECLUSAGE_TEXCOORD && e.UsageIndex==contract.coordinates && e.Type==D3DDECLTYPE_FLOAT2) uv=e.Offset;
  }
  if(pos<0 || color<0 || uv<0 || UINT(pos+12)>stride || UINT(color+4)>stride || UINT(uv+8)>stride ||
     (channels&&normal<0)||(normal>=0 && UINT(normal+12)>stride)) return SurfaceSubmitFailure(__LINE__);
  const UINT indexSize=id.Format==D3DFMT_INDEX16?2:4,indexCount=type==D3DPT_TRIANGLELIST?count*3:count+2;
  if(uint64_t(start+uint64_t(indexCount))*indexSize>id.Size) return SurfaceSubmitFailure(__LINE__);
  const void* indexData=indexBytes.data()+start*indexSize;
  std::vector<uint32_t> indices(indexCount);
  bool valid=true;
  for(UINT i=0;i<indexCount;++i) {
    const uint32_t original=indexSize==2?static_cast<const uint16_t*>(indexData)[i]:static_cast<const uint32_t*>(indexData)[i];
    const int64_t effective=int64_t(base)+original;
    if(original<minVertex || uint64_t(original)>=uint64_t(minVertex)+vertices || effective<0 ||
       uint64_t(offset)+(uint64_t(effective)+1)*stride>vd.Size) {valid=false;break;}
    indices[i]=static_cast<uint32_t>(effective);
  }
  if(!valid) return SurfaceSubmitFailure(__LINE__);
  const void* vertexData=vertexBytes.data();
  std::vector<remixapi_HardcodedVertex> expanded;expanded.reserve(size_t(count)*3);
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
  if(!valid || expanded.empty()) return SurfaceSubmitFailure(__LINE__);
  material_channels::Plan plan;
  if(channels) {
    bool uniform=true;const auto firstColor=expanded.front().color;
    for(const auto& v:expanded)if((v.color&0xffffffu)!=(firstColor&0xffffffu)){uniform=false;break;}
    if(!material_channels::Factor(*channels,uniform,firstColor,plan))return SurfaceSubmitFailure(__LINE__);
    for(auto& v:expanded)v.color=material_channels::Vertex(v.color,plan);
  }
  const auto material=channels?SurfaceChannelMaterial(d,textureHash,plan):SurfaceMaterial(d,textureHash);
  if(!material) return SurfaceSubmitFailure(__LINE__);
  const auto hash=XXH3_64bits_withSeed(expanded.data(),expanded.size()*sizeof(expanded[0]),reinterpret_cast<uintptr_t>(material));
  auto found=surfaceMeshes.find(hash);
  if(found==surfaceMeshes.end()) {
    const size_t bytes=expanded.size()*(sizeof(expanded[0])+4);
    std::vector<uint32_t> sequential(expanded.size());for(size_t i=0;i<sequential.size();++i) sequential[i]=static_cast<uint32_t>(i);
    remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=expanded.data();surface.vertices_count=expanded.size();
    surface.indices_values=sequential.data();surface.indices_count=sequential.size();surface.material=material;
    remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;info.hash=hash;info.surfaces_values=&surface;info.surfaces_count=1;
    if(!EnsureSurfaceResourceRoom(1,bytes,0,material))return SurfaceSubmitFailure(__LINE__);
    remixapi_MeshHandle mesh=nullptr;if(api->CreateMesh(&info,&mesh)!=REMIXAPI_ERROR_CODE_SUCCESS || !mesh) return SurfaceSubmitFailure(__LINE__);
    found=surfaceMeshes.emplace(hash,SurfaceMesh{mesh,frameId,bytes,material}).first;surfaceMeshBytes+=bytes;++surfaceMeshCreates;
  }
  found->second.frame=frameId;
  D3DMATRIX world{};DWORD test=0,func=0,ref=0,mask=0;
  if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) || FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&test)) ||
     FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&func)) || FAILED(d->GetRenderState(D3DRS_ALPHAREF,&ref)) ||
     FAILED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&mask)) || func<1 || func>8) return SurfaceSubmitFailure(__LINE__);
  if(nativeInput)world=nativeGeometry.world;
  remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;
  if(!surface_material::AlphaTest(test!=0,ref,func,blend))return SurfaceSubmitFailure(__LINE__);
  DWORD enabled=1,src=D3DBLEND_SRCALPHA,dst=D3DBLEND_INVSRCALPHA,op=D3DBLENDOP_ADD;
  if(channels&&(FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&enabled))||FAILED(d->GetRenderState(D3DRS_SRCBLEND,&src))||
     FAILED(d->GetRenderState(D3DRS_DESTBLEND,&dst))||FAILED(d->GetRenderState(D3DRS_BLENDOP,&op))))return SurfaceSubmitFailure(__LINE__);
  if(channels&&enabled&&(op!=D3DBLENDOP_ADD||
     !((src==D3DBLEND_ONE&&dst==D3DBLEND_ZERO)||(src==D3DBLEND_SRCALPHA&&dst==D3DBLEND_INVSRCALPHA))))return SurfaceSubmitFailure(__LINE__);
  blend.alphaBlendEnabled=enabled;blend.srcColorBlendFactor=src==D3DBLEND_ONE?1:6;blend.dstColorBlendFactor=dst==D3DBLEND_ZERO?0:7;
  blend.srcAlphaBlendFactor=blend.srcColorBlendFactor;blend.dstAlphaBlendFactor=blend.dstColorBlendFactor;blend.writeMask=mask;
  surface_material::Apply(contract,blend);
  remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&blend;
  instance.categoryFlags=channels?0:REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC;instance.mesh=found->second.handle;instance.doubleSided=cull==D3DCULL_NONE;
  for(unsigned r=0;r<3;++r) for(unsigned c=0;c<4;++c) {
    const float value=world.m[c][r];if(!std::isfinite(value)) return SurfaceSubmitFailure(__LINE__);instance.transform.matrix[r][c]=value;
  }
  const bool submitted=api->DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS;
  if(submitted&&nativeInput) {
    ++native_mesh_source::used;
    // One sampled frame per F8 request, plus periodic frames. A 120-frame
    // trace of every native instance otherwise exhausts the log during A/B.
    if(native_mesh_source::output&&_ftelli64(native_mesh_source::output)<16*1024*1024&&(frameId%300==0||frameId+120==traceUntilFrame))
      fprintf(native_mesh_source::output,"{\"event\":\"submit\",\"frame\":%u,\"draw\":%u,\"mesh\":%u,\"submission\":%llu,\"generation\":%llu,\"geometrySource\":\"%s\",\"worldSource\":\"native_renderer\",\"layoutSource\":\"native_flags_shared_emitter\",\"componentFlags\":%u,\"materialSource\":\"d3d_state\"}\n",
        frameId,drawId,nativeGeometry.mesh,nativeGeometry.submission,nativeGeometry.vertices->generation,nativeGeometry.vertices->source,nativeGeometry.componentFlags);
  }
  if(submitted&&channels&&surfaceRoleLog&&(frameId%300==0||frameId<traceUntilFrame)) {
    fprintf(surfaceRoleLog,"{\"event\":\"material_channels\",\"frame\":%u,\"draw\":%u,\"albedo\":[%.9g,%.9g,%.9g],\"emission\":[%.9g,%.9g,%.9g],\"vertexRGB\":%s,\"vertexAlpha\":%s,\"alpha\":%u}\n",
      frameId,drawId,plan.albedo.v[0],plan.albedo.v[1],plan.albedo.v[2],plan.emission.v[0],plan.emission.v[1],plan.emission.v[2],plan.vertexRGB?"true":"false",plan.vertexAlpha?"true":"false",plan.alpha);
  }
  if(submitted && surfaceRoleLog && (frameId%300==0 || frameId<traceUntilFrame)) {
    fprintf(surfaceRoleLog,"{\"event\":\"submit_material\",\"frame\":%u,\"draw\":%u,\"rgb\":[%u,%u,%u],\"alpha\":[%u,%u,%u],\"factor\":%lu,\"uv\":%lu,\"transform\":%lu,\"nativeAlphaTest\":[%lu,%lu,%lu],\"apiAlphaCompare\":%u}\n",
      frameId,drawId,contract.rgb.operation,contract.rgb.first,contract.rgb.second,
      contract.alpha.operation,contract.alpha.first,contract.alpha.second,contract.factor,contract.coordinates,contract.transformFlags,
      test,func,ref,blend.alphaTestCompareOp);
  }
  return submitted;
}
