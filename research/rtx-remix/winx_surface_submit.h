// Own bounded translation of a recognized FFP overlay into explicit Remix API
// resources, including separate FFP color coefficients. No asset IDs or game logic.
#include "winx_surface_material.h"
#include "winx_material_channels.h"
#include "winx_material_channel_assets.h"
#include "winx_native_material_source.h"
#include "winx_geometry_probe.h"
#include <algorithm>
static bool materialChannelsEnabled,keepMaterialChannelsForComparison;
static bool preserveUnlitColor=true;
static unsigned materialChannelsSubmitted,materialChannelsRejected;
static wchar_t surfaceAssetDirectory[MAX_PATH]{};
struct SurfaceMesh {
  remixapi_MeshHandle handle;unsigned frame;size_t bytes;remixapi_MaterialHandle material;
  unsigned failedPressureFrame=0;bool pressureFailed=false;
  bool usable=true; // false while pending or quarantined after a failed Create
};
struct SurfaceMaterialEntry {
  remixapi_MaterialHandle handle;unsigned frame;
  unsigned failedPressureFrame=0;bool pressureFailed=false;
  bool usable=true; // false while pending or quarantined after a failed Create
};
static std::map<uint64_t,SurfaceMesh> surfaceMeshes;
static std::map<uint64_t,SurfaceMaterialEntry> surfaceMaterials;
static size_t surfaceMeshBytes;
static unsigned surfaceMeshCreates,surfaceMeshDestroys,surfaceMaterialCreates,surfaceMaterialDestroys,surfaceResourceFailures;
static unsigned surfacePressureRequests,surfacePressureMeshDestroys,surfacePressureMaterialDestroys,surfacePressureRejected;
static constexpr size_t surfaceMeshLimit=512,surfaceMaterialLimit=256,surfaceMeshByteLimit=64*1024*1024;

// The existing recursive guard protects cache nodes; resource/API reentry is
// rejected while an operation owns them. Retire requests are never discarded:
// they invalidate the current result and drain once at the outer boundary.
static bool surfaceResourceBusy,surfaceRetirePending,surfaceRetireAll,surfaceRetireDraining;
static uint64_t surfaceOwnershipEpoch=1,surfaceRetireSerial;
static unsigned surfaceResourceReentryRejected;
static void RetireSurfaceResources(bool all=false);
struct SurfaceResourceOperation {
  std::unique_lock<std::recursive_mutex> lock{guard};
  uint64_t retireSerial=surfaceRetireSerial;
  unsigned frame=frameId;
  bool owned=false;
  SurfaceResourceOperation() {
    if(surfaceResourceBusy){++surfaceResourceReentryRejected;return;}
    if(surfaceRetirePending&&!surfaceRetireDraining) {
      const bool all=surfaceRetireAll;surfaceRetirePending=surfaceRetireAll=false;
      surfaceRetireDraining=true;RetireSurfaceResources(all);surfaceRetireDraining=false;
      if(surfaceRetirePending){++surfaceResourceReentryRejected;return;}
    }
    retireSerial=surfaceRetireSerial;frame=frameId;surfaceResourceBusy=owned=true;
  }
  bool Stable() const {return owned&&retireSerial==surfaceRetireSerial&&frame==frameId;}
  ~SurfaceResourceOperation() {
    if(!owned)return;
    surfaceResourceBusy=false;
    if(surfaceRetirePending&&!surfaceRetireDraining) {
      const bool all=surfaceRetireAll;surfaceRetirePending=surfaceRetireAll=false;
      surfaceRetireDraining=true;RetireSurfaceResources(all);surfaceRetireDraining=false;
    }
  }
};
static uint64_t SurfaceResourceEpoch() {std::lock_guard<std::recursive_mutex> lock(guard);return surfaceOwnershipEpoch;}
static bool SurfaceMeshCurrent(remixapi_MeshHandle handle,remixapi_MaterialHandle material=nullptr) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(surfaceResourceBusy||surfaceRetirePending||!handle)return false;
  for(const auto& item:surfaceMeshes)if(item.second.handle==handle&&item.second.usable&&
      (!material||item.second.material==material)) {
    for(const auto& entry:surfaceMaterials)if(entry.second.handle==item.second.material&&entry.second.usable)return true;
    return false;
  }
  return false;
}

struct SurfaceBuffer { std::vector<uint8_t> bytes; bool complete=false; };
struct SurfaceWrite { const void* data; UINT offset,size,total; DWORD flags; };
static std::map<void*,SurfaceBuffer> surfaceBuffers;
static std::map<void*,SurfaceWrite> surfaceWrites;
static size_t surfaceBufferBytes;
#include "winx_native_skin_vertex_source.h"
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

#include "winx_surface_geometry.h"
#include "winx_surface_instance.h"
#include "winx_independent_scene_source.h"

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

// All helpers below run under an owned SurfaceResourceOperation. No cache
// iterator/reference crosses a COM/API call, including deletion failures.
static bool SurfaceMaterialReferenced(remixapi_MaterialHandle handle) {
  for(const auto& item:surfaceMeshes)if(item.second.material==handle)return true;
  return false;
}
static bool DestroySurfaceMesh(uint64_t key,bool pressure=false) {
  SurfaceMesh saved{};
  {const auto it=surfaceMeshes.find(key);if(it==surfaceMeshes.end()||!it->second.handle)return false;saved=it->second;}
  auto api=GetRemixApi();remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {if(api&&api->DestroyMesh)result=api->DestroyMesh(saved.handle);}catch(const std::bad_alloc&){}
  auto it=surfaceMeshes.find(key);
  if(it==surfaceMeshes.end()||it->second.handle!=saved.handle)return false;
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS) {
    ++surfaceResourceFailures;it->second.pressureFailed=true;it->second.failedPressureFrame=frameId;return false;
  }
  ++surfaceMeshDestroys;if(pressure)++surfacePressureMeshDestroys;
  surfaceMeshBytes-=it->second.bytes;surfaceMeshes.erase(it);++surfaceOwnershipEpoch;return true;
}
static bool DestroySurfaceMaterial(uint64_t key,bool pressure=false) {
  SurfaceMaterialEntry saved{};
  {const auto it=surfaceMaterials.find(key);if(it==surfaceMaterials.end()||!it->second.handle||SurfaceMaterialReferenced(it->second.handle))return false;saved=it->second;}
  auto api=GetRemixApi();remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {if(api&&api->DestroyMaterial)result=api->DestroyMaterial(saved.handle);}catch(const std::bad_alloc&){}
  auto it=surfaceMaterials.find(key);
  if(it==surfaceMaterials.end()||it->second.handle!=saved.handle)return false;
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS) {
    ++surfaceResourceFailures;it->second.pressureFailed=true;it->second.failedPressureFrame=frameId;return false;
  }
  ++surfaceMaterialDestroys;if(pressure)++surfacePressureMaterialDestroys;
  surfaceMaterials.erase(it);++surfaceOwnershipEpoch;return true;
}
static void RetireSurfaceResources(bool all) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(!all&&frameId%30)return;
  ++surfaceOwnershipEpoch;++surfaceRetireSerial;
  if(surfaceResourceBusy){surfaceRetirePending=true;surfaceRetireAll|=all;return;}
  if(surfaceMeshes.empty()&&surfaceMaterials.empty())return;
  SurfaceResourceOperation operation;if(!operation.owned)return;
  auto api=GetRemixApi();if(!api||!api->DestroyMesh||!api->DestroyMaterial)return;
  uint64_t meshKeys[surfaceMeshLimit]{},materialKeys[surfaceMaterialLimit]{};size_t meshes=0,materials=0;
  for(auto& item:surfaceMeshes) {
    if(all)item.second.frame=frameId-301u;
    if(frameId-item.second.frame>300&&meshes<surfaceMeshLimit)meshKeys[meshes++]=item.first;
  }
  for(size_t i=0;i<meshes;++i)DestroySurfaceMesh(meshKeys[i]);
  for(auto& item:surfaceMaterials) {
    if(all)item.second.frame=frameId-301u;
    if(frameId-item.second.frame>300&&materials<surfaceMaterialLimit)materialKeys[materials++]=item.first;
  }
  for(size_t i=0;i<materials;++i)DestroySurfaceMaterial(materialKeys[i]);
}
// Pressure never retires current-frame resources, including complete groups
// being prepared for independent submission. Fixed scratch and no heap work.
static bool EnsureSurfaceResourceRoomOwned(size_t meshes,size_t bytes,size_t materials,
                                          remixapi_MaterialHandle protectedMaterial=nullptr) {
  const auto room=[&]() {
    return meshes<=surfaceMeshLimit&&materials<=surfaceMaterialLimit&&bytes<=surfaceMeshByteLimit&&
      surfaceMeshes.size()<=surfaceMeshLimit-meshes&&surfaceMaterials.size()<=surfaceMaterialLimit-materials&&
      surfaceMeshBytes<=surfaceMeshByteLimit-bytes;
  };
  if(room())return true;
  ++surfacePressureRequests;
  if(meshes>surfaceMeshLimit||materials>surfaceMaterialLimit||bytes>surfaceMeshByteLimit) {++surfacePressureRejected;return false;}
  auto api=GetRemixApi();if(!api||!api->DestroyMesh||!api->DestroyMaterial){++surfacePressureRejected;return false;}
  const auto orphans=[&]() {
    uint64_t keys[surfaceMaterialLimit]{};size_t count=0;
    for(const auto& item:surfaceMaterials) {
      const auto& value=item.second;
      if(value.frame!=frameId&&value.handle!=protectedMaterial&&
         !(value.pressureFailed&&value.failedPressureFrame==frameId)&&!SurfaceMaterialReferenced(value.handle)&&count<surfaceMaterialLimit)
        keys[count++]=item.first;
    }
    for(size_t i=0;i<count;++i){DestroySurfaceMaterial(keys[i],true);if(room())break;}
  };
  orphans();if(room())return true;
  struct Candidate {uint64_t key;unsigned age;};
  Candidate candidates[surfaceMeshLimit]{};size_t count=0;
  for(const auto& item:surfaceMeshes) {
    const auto& value=item.second;
    if(value.frame!=frameId&&!(value.pressureFailed&&value.failedPressureFrame==frameId)&&count<surfaceMeshLimit)
      candidates[count++]={item.first,frameId-value.frame};
  }
  std::sort(candidates,candidates+count,[](const Candidate& a,const Candidate& b){return a.age!=b.age?a.age>b.age:a.key<b.key;});
  for(size_t index=0;index<count&&!room();++index) {
    remixapi_MaterialHandle referenced=nullptr;
    {const auto it=surfaceMeshes.find(candidates[index].key);if(it==surfaceMeshes.end())continue;referenced=it->second.material;}
    if(surfaceMeshes.size()<=surfaceMeshLimit-meshes&&surfaceMeshBytes<=surfaceMeshByteLimit-bytes) {
      bool eligible=false;
      for(const auto& entry:surfaceMaterials)if(entry.second.handle==referenced) {
        const auto& material=entry.second;
        eligible=material.frame!=frameId&&material.handle!=protectedMaterial&&
          !(material.pressureFailed&&material.failedPressureFrame==frameId);break;
      }
      if(eligible)for(const auto& mesh:surfaceMeshes)if(mesh.second.material==referenced&&mesh.second.frame==frameId){eligible=false;break;}
      if(!eligible)continue;
    }
    if(DestroySurfaceMesh(candidates[index].key,true))orphans();
  }
  const bool result=room();if(!result)++surfacePressureRejected;return result;
}
static bool EnsureSurfaceResourceRoom(size_t meshes,size_t bytes,size_t materials,
                                      remixapi_MaterialHandle protectedMaterial=nullptr) {
  SurfaceResourceOperation operation;if(!operation.owned)return false;
  return EnsureSurfaceResourceRoomOwned(meshes,bytes,materials,protectedMaterial)&&operation.Stable();
}
// Both material entry points share the ownership boundary. Cache allocation
// happens before API Create; even a failing Create that writes a handle is owned.
static remixapi_MaterialHandle CreateSurfaceMaterialOwned(const remixapi_MaterialInfo& info,const SurfaceResourceOperation& operation) {
  if(!operation.Stable())return nullptr;
  {auto it=surfaceMaterials.find(info.hash);if(it!=surfaceMaterials.end()) {
    if(!it->second.usable)return nullptr;it->second.frame=frameId;return it->second.handle;
  }}
  auto api=GetRemixApi();if(!api||!api->CreateMaterial||!api->DestroyMaterial||
      !EnsureSurfaceResourceRoomOwned(0,0,1)||!operation.Stable())return nullptr;
  try {
    if(!surfaceMaterials.emplace(info.hash,SurfaceMaterialEntry{nullptr,frameId,0,false,false}).second)return nullptr;
  } catch(const std::bad_alloc&) {++surfaceResourceFailures;return nullptr;}
  ++surfaceOwnershipEpoch;
  remixapi_MaterialHandle handle=nullptr;remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {result=api->CreateMaterial(&info,&handle);}catch(const std::bad_alloc&){}
  {auto it=surfaceMaterials.find(info.hash);
    if(!handle){surfaceMaterials.erase(it);++surfaceOwnershipEpoch;++surfaceResourceFailures;return nullptr;}
    ++surfaceMaterialCreates;it->second.handle=handle;
    it->second.usable=result==REMIXAPI_ERROR_CODE_SUCCESS&&operation.Stable();
    if(it->second.usable)return handle;
    it->second.frame=frameId-301u;
  }
  ++surfaceResourceFailures;DestroySurfaceMaterial(info.hash);return nullptr;
}
static void SetPreserveUnlitColor(bool value) {
  if(value==preserveUnlitColor)return;
  // A policy switch invalidates every material coefficient. Retire old meshes
  // before their materials so the two variants cannot exhaust the bounded
  // cache and force a partially translated scene during live comparison.
  RetireSurfaceResources(true);preserveUnlitColor=value;
}

static remixapi_MaterialHandle SurfaceMaterial(IDirect3DDevice9* d,uint64_t textureHash,const native_material_source::Sampler* nativeSampler=nullptr) {
  try {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  DWORD u=0,v=0,mag=0;
  if(nativeSampler){u=nativeSampler->u;v=nativeSampler->v;mag=nativeSampler->mag;}
  else if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u)) || FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v)) ||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag))) return nullptr;
  if(u<1 || u>3 || v<1 || v>3 || mag<1 || mag>3)return nullptr;
  const uint64_t descriptor[]={textureHash,u,v,mag};
  const auto hash=XXH3_64bits(descriptor,sizeof(descriptor));
  {auto found=surfaceMaterials.find(hash);if(found!=surfaceMaterials.end()) {if(!found->second.usable||!operation.Stable())return nullptr;found->second.frame=frameId;return found->second.handle;}}
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
  return CreateSurfaceMaterialOwned(info,operation);
  }catch(const std::bad_alloc&){++surfaceResourceFailures;return nullptr;}
}

static remixapi_MaterialHandle SurfaceChannelMaterial(IDirect3DDevice9* d,uint64_t textureHash,const material_channels::Plan& plan,
                                                       const native_material_source::Sampler* nativeSampler=nullptr,
                                                       IDirect3DBaseTexture9* textureSource=nullptr,const DWORD* sourceSrgb=nullptr) {
  try {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  DWORD u=0,v=0,mag=0,srgb=0;
  if(nativeSampler){u=nativeSampler->u;v=nativeSampler->v;mag=nativeSampler->mag;}
  else if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u))||FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v))||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag)))return nullptr;
  if(sourceSrgb)srgb=*sourceSrgb;
  else if(FAILED(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb)))return nullptr;
  if(srgb||
     u<1||u>3||v<1||v>3||mag<1||mag>3||!surfaceAssetDirectory[0])return nullptr;
  std::string descriptor="winx-independent-ffp-v1";
  const uint64_t values[]={textureHash,u,v,mag};descriptor.append(reinterpret_cast<const char*>(values),sizeof(values));
  descriptor.append(reinterpret_cast<const char*>(&plan.albedo),sizeof(plan.albedo));
  descriptor.append(reinterpret_cast<const char*>(&plan.emission),sizeof(plan.emission));
  const auto hash=XXH3_64bits(descriptor.data(),descriptor.size());
  {auto found=surfaceMaterials.find(hash);if(found!=surfaceMaterials.end()){if(!found->second.usable||!operation.Stable())return nullptr;found->second.frame=frameId;return found->second.handle;}}
  wchar_t albedo[MAX_PATH]{},emission[MAX_PATH]{};
  swprintf_s(albedo,L"%s\\%016llX-albedo.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  swprintf_s(emission,L"%s\\%016llX-emission.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  const bool emissive=!material_channels::Zero(plan.emission);
  const auto write=[&](const wchar_t* path,const material_channels::RGB& tint){
    return textureSource?material_channels::WriteTextureSource(textureSource,path,tint):material_channels::WriteTexture(d,path,tint);
  };
  if(!write(albedo,plan.albedo)||(emissive&&!write(emission,plan.emission)))return nullptr;
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=.5f;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;info.hash=hash;
  info.albedoTexture=albedo;info.emissiveTexture=emissive?emission:nullptr;info.emissiveIntensity=emissive?1.f:0.f;
  info.wrapModeU=static_cast<uint8_t>(u-1);info.wrapModeV=static_cast<uint8_t>(v-1);info.filterMode=mag==D3DTEXF_POINT?0:1;
  return CreateSurfaceMaterialOwned(info,operation);
  }catch(const std::bad_alloc&){++surfaceResourceFailures;return nullptr;}
}

// One resource cache for both geometry sources. Instance transforms and draw
// state deliberately do not participate in this immutable mesh resource key.
static remixapi_MeshHandle SurfaceGeometryResource(const std::vector<remixapi_HardcodedVertex>& expanded,
                                                  remixapi_MaterialHandle material) {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  const auto api=GetRemixApi();if(!api||!api->CreateMesh||!api->DestroyMesh||!material||expanded.empty())return nullptr;
  const auto hash=XXH3_64bits_withSeed(expanded.data(),expanded.size()*sizeof(expanded[0]),reinterpret_cast<uintptr_t>(material));
  {auto found=surfaceMeshes.find(hash);if(found!=surfaceMeshes.end()) {
    if(!found->second.usable||!operation.Stable())return nullptr;found->second.frame=frameId;return found->second.handle;
  }}
  if(expanded.size()>surfaceMeshByteLimit/(sizeof(expanded[0])+4))return nullptr;
  const size_t bytes=expanded.size()*(sizeof(expanded[0])+4);
  std::vector<uint32_t> sequential;
  try {sequential.resize(expanded.size());}catch(const std::bad_alloc&){++surfaceResourceFailures;return nullptr;}
  for(size_t i=0;i<sequential.size();++i)sequential[i]=static_cast<uint32_t>(i);
  remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=expanded.data();surface.vertices_count=expanded.size();
  surface.indices_values=sequential.data();surface.indices_count=sequential.size();surface.material=material;
  remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;info.hash=hash;info.surfaces_values=&surface;info.surfaces_count=1;
  if(!EnsureSurfaceResourceRoomOwned(1,bytes,0,material)||!operation.Stable())return nullptr;
  try {
    if(!surfaceMeshes.emplace(hash,SurfaceMesh{nullptr,frameId,bytes,material,0,false,false}).second)return nullptr;
  }catch(const std::bad_alloc&){++surfaceResourceFailures;return nullptr;}
  surfaceMeshBytes+=bytes;++surfaceOwnershipEpoch;
  remixapi_MeshHandle handle=nullptr;remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {result=api->CreateMesh(&info,&handle);}catch(const std::bad_alloc&){}
  {auto it=surfaceMeshes.find(hash);
    if(!handle){surfaceMeshBytes-=it->second.bytes;surfaceMeshes.erase(it);++surfaceOwnershipEpoch;++surfaceResourceFailures;return nullptr;}
    ++surfaceMeshCreates;it->second.handle=handle;
    it->second.usable=result==REMIXAPI_ERROR_CODE_SUCCESS&&operation.Stable();
    if(it->second.usable)return handle;
    it->second.frame=frameId-301u;
  }
  ++surfaceResourceFailures;DestroySurfaceMesh(hash);return nullptr;
}

static bool SurfaceSubmitFailure(unsigned line) {
  static std::set<unsigned> reported;
  if(surfaceRoleLog && reported.insert(line).second) {
    fprintf(surfaceRoleLog,"{\"event\":\"submit_unsupported\",\"frame\":%u,\"line\":%u}\n",frameId,line);fflush(surfaceRoleLog);
  }
  return false;
}

static bool SubmitSurfaceOverlay(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minVertex,UINT vertices,
                                 UINT start,UINT count,uint64_t textureHash,const material_channels::Input* channels=nullptr,
                                 const ScopedOpaqueAlphaTest* alphaNormalization=nullptr) {
  bool submitted=false;
  try {
  if(channels&&independent_scene_source::TrySelectedDraw(d,{type,base,minVertex,vertices,start,count},alphaNormalization))return true;
  auto api=GetRemixApi();
  if(!api || !api->CreateMesh || !api->DrawInstance || !api->CreateMaterial || count>32768 || vertices>65536) return SurfaceSubmitFailure(__LINE__);
  surface_material::Contract contract;
  surface_material::ObservedStage observedStage{};
  DWORD cull=0,separateAlpha=0,stencil=0;
  if(!(channels?surface_material::ReadTexture(d,contract,&observedStage):surface_material::Read(d,contract,&observedStage))||
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
  // bytes, range, world and recovered vertex layout. A separately qualified
  // native material snapshot can replace its supported inputs below.
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
  native_material_source::Packet nativeMaterial{};
  const bool nativeMaterialMatched=nativeInput&&native_material_source::Resolve(d,nativeGeometry,contract,observedStage,channels,preserveUnlitColor,nativeMaterial);
  const bool nativeMaterialInput=nativeMaterialMatched&&native_material_source::submitEnabled;
  if(nativeMaterialInput){contract=nativeMaterial.contract;channels=&nativeMaterial.channels;}
  nativeInput=nativeInput&&native_mesh_source::submitEnabled;
  const auto& vertexBytes=nativeInput?nativeGeometry.vertices->data:vbSnapshot->second.bytes;
  const auto& indexBytes=nativeInput?nativeGeometry.indices->data:ibSnapshot->second.bytes;
  if(nativeInput){const auto& range=nativeGeometry.range;type=range.type;base=range.base;minVertex=range.minimum;
    vertices=range.vertices;start=range.start;count=range.count;stride=nativeGeometry.stride;}
  if(nativeInput) {
    n=UINT(nativeGeometry.layout->size());
    memcpy(layout,nativeGeometry.layout->data(),size_t(n)*sizeof(layout[0]));
  }
  SurfaceGeometryInput conversion{};
  conversion.vertices=&vertexBytes;conversion.indices=&indexBytes;conversion.elements=layout;conversion.elementCount=n;
  conversion.stride=stride;conversion.offset=offset;conversion.indexSize=id.Format==D3DFMT_INDEX16?2:4;
  conversion.range={type,base,minVertex,vertices,start,count};conversion.cull=cull;conversion.contract=&contract;conversion.channels=channels;
  std::vector<remixapi_HardcodedVertex> expanded;material_channels::Plan plan;
  if(!ExpandSurfaceGeometry(conversion,expanded,plan))return SurfaceSubmitFailure(__LINE__);
  const auto sampler=nativeMaterialInput?&nativeMaterial.sampler:nullptr;
  const auto material=channels?SurfaceChannelMaterial(d,textureHash,plan,sampler):SurfaceMaterial(d,textureHash,sampler);
  if(!material) return SurfaceSubmitFailure(__LINE__);
  const auto meshHandle=SurfaceGeometryResource(expanded,material);
  if(!meshHandle)return SurfaceSubmitFailure(__LINE__);
  D3DMATRIX world{};DWORD test=0,func=0,ref=0,mask=0;
  if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) || FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&test)) ||
     FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&func)) || FAILED(d->GetRenderState(D3DRS_ALPHAREF,&ref)) ||
     FAILED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&mask)) || func<1 || func>8) return SurfaceSubmitFailure(__LINE__);
  if(nativeInput)world=nativeGeometry.world;
  DWORD enabled=1,src=D3DBLEND_SRCALPHA,dst=D3DBLEND_INVSRCALPHA,op=D3DBLENDOP_ADD;
  if(channels&&(FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&enabled))||FAILED(d->GetRenderState(D3DRS_SRCBLEND,&src))||
     FAILED(d->GetRenderState(D3DRS_DESTBLEND,&dst))||FAILED(d->GetRenderState(D3DRS_BLENDOP,&op))))return SurfaceSubmitFailure(__LINE__);
  SurfaceInstanceState instanceState{cull,test,func,ref,mask,enabled,src,dst,op,channels!=nullptr};
  remixapi_InstanceInfoBlendEXT blend{};remixapi_InstanceInfo instance{};
  if(!DescribeSurfaceInstance(instanceState,contract,world,meshHandle,blend,instance))return SurfaceSubmitFailure(__LINE__);
  submitted=api->DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS;
  if(submitted&&nativeInput&&native_owner_source::Qualify(nativeGeometry.owner,nativeGeometry.mesh,nativeGeometry.renderer,
      nativeGeometry.submission,nativeGeometry.world)) {
    nativeGeometry.owner.geometryGeneration=nativeGeometry.vertices->generation;
    native_owner_source::RecordUse(nativeGeometry.owner,nativeMaterialInput);
    independent_scene_source::Compare(nativeGeometry.owner,nativeMaterialMatched?&nativeMaterial:nullptr,d,alphaNormalization,&nativeGeometry);
  }
  if(submitted&&nativeMaterialInput)native_material_source::RecordUse(nativeGeometry,nativeMaterial);
  if(submitted&&nativeInput) {
    ++native_mesh_source::used;
    // One sampled frame per F8 request, plus periodic frames. A 120-frame
    // trace of every native instance otherwise exhausts the log during A/B.
    if(native_mesh_source::output&&_ftelli64(native_mesh_source::output)<16*1024*1024&&(frameId%300==0||frameId+120==traceUntilFrame))
      fprintf(native_mesh_source::output,"{\"event\":\"submit\",\"frame\":%u,\"draw\":%u,\"mesh\":%u,\"submission\":%llu,\"generation\":%llu,\"geometrySource\":\"%s\",\"worldSource\":\"native_renderer\",\"layoutSource\":\"native_flags_shared_emitter\",\"componentFlags\":%u,\"materialSource\":\"%s\"}\n",
        frameId,drawId,nativeGeometry.mesh,nativeGeometry.submission,nativeGeometry.vertices->generation,nativeGeometry.vertices->source,nativeGeometry.componentFlags,
        nativeMaterialInput?"native_std_layer_shared_mapping":"d3d_state");
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
  }catch(const std::bad_alloc&){
    // Diagnostic/native comparison allocation can fail after the API draw.
    // Preserve that committed result so the intercepted D3D draw is not repeated.
    ++surfaceResourceFailures;return submitted;
  }
}
#include "winx_independent_scene_submit.h"
#include "winx_selected_scene_submit.h"
