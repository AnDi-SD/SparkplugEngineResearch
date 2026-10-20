// Own bounded translation of a recognized FFP overlay into explicit Remix API
// resources, including separate FFP color coefficients. Resource identities use
// our content protocol; original game logic is not implemented here.
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
#include "winx_remix_resource_registry.h"
static winx_remix::RemixResourceRegistry surfaceResources(guard,frameId,&GetRemixApi);

// Transitional proxy facade. API ownership belongs to the standalone registry;
// D3D capture, material conversion, hashing and native qualification stay here.
struct SurfaceResourceOperation {
  winx_remix::RemixResourceRegistry::Operation scope{surfaceResources};
  const bool& owned=scope.owned;
  bool Stable() const {return scope.Stable();}
};
static void RetireSurfaceResources(bool all=false) {surfaceResources.Retire(all);}
static uint64_t SurfaceResourceEpoch() {return surfaceResources.Epoch();}
static bool SurfaceMeshCurrent(remixapi_MeshHandle mesh,remixapi_MaterialHandle material=nullptr) {
  return surfaceResources.MeshCurrent(mesh,material);
}

#ifdef WINX_REMIX_TEST
// Existing fault/pressure fixtures inject known cache records. Production only
// receives const views and mutations through registry operations.
namespace winx_remix {
struct RemixResourceRegistryTestAccess {
  static auto& Meshes(RemixResourceRegistry& registry) {return registry.meshes_;}
  static auto& Materials(RemixResourceRegistry& registry) {return registry.materials_;}
  static auto& MeshBytes(RemixResourceRegistry& registry) {return registry.stats_.meshBytes;}
};
}
using SurfaceMesh=winx_remix::RemixResourceRegistry::MeshEntry;
using SurfaceMaterialEntry=winx_remix::RemixResourceRegistry::MaterialEntry;
static auto& surfaceMeshes=winx_remix::RemixResourceRegistryTestAccess::Meshes(surfaceResources);
static auto& surfaceMaterials=winx_remix::RemixResourceRegistryTestAccess::Materials(surfaceResources);
static auto& surfaceMeshBytes=winx_remix::RemixResourceRegistryTestAccess::MeshBytes(surfaceResources);
static const auto& surfaceMeshCreates=surfaceResources.Stats().meshCreates;
static const auto& surfaceMeshDestroys=surfaceResources.Stats().meshDestroys;
static const auto& surfaceMaterialCreates=surfaceResources.Stats().materialCreates;
static const auto& surfaceMaterialDestroys=surfaceResources.Stats().materialDestroys;
static const auto& surfaceResourceFailures=surfaceResources.Stats().resourceFailures;
static const auto& surfacePressureRequests=surfaceResources.Stats().pressureRequests;
static const auto& surfacePressureMeshDestroys=surfaceResources.Stats().pressureMeshDestroys;
static const auto& surfacePressureMaterialDestroys=surfaceResources.Stats().pressureMaterialDestroys;
static const auto& surfacePressureRejected=surfaceResources.Stats().pressureRejected;
static const auto& surfaceResourceReentryRejected=surfaceResources.Stats().reentryRejected;
static constexpr size_t surfaceMeshLimit=winx_remix::RemixResourceRegistry::MeshLimit;
static constexpr size_t surfaceMaterialLimit=winx_remix::RemixResourceRegistry::MaterialLimit;
static constexpr size_t surfaceMeshByteLimit=winx_remix::RemixResourceRegistry::MeshByteLimit;
#endif

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
#include "winx_resource_identity.h"

// The byte protocol belongs to ResourceIdentitySource. XXH3 is the existing
// adapter digest, not a native ID or a guarantee against hash collisions.
struct SurfaceIdentityHash {
  XXH3_state_t state{};
  void Reset(){XXH3_64bits_reset(&state);}
  void Update(const void* bytes,size_t size){XXH3_64bits_update(&state,bytes,size);}
  uint64_t Digest(){return XXH3_64bits_digest(&state);}
};
static uint64_t SurfaceChannelMaterialIdentity(uint64_t textureHash,DWORD u,DWORD v,DWORD mag,
                                               const material_channels::Plan& plan) {
  winx_remix::ResourceIdentitySource::MaterialRequest input{};
  input.textureHash=textureHash;input.u=u;input.v=v;input.mag=mag;
  memcpy(input.albedo,plan.albedo.v,sizeof(input.albedo));
  memcpy(input.emission,plan.emission.v,sizeof(input.emission));
  SurfaceIdentityHash hash;
  return winx_remix::ResourceIdentitySource::Material(input,hash);
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

static bool EnsureSurfaceResourceRoom(size_t meshes,size_t bytes,size_t materials,
                                     remixapi_MaterialHandle protectedMaterial=nullptr) {
  return surfaceResources.EnsureRoom(meshes,bytes,materials,protectedMaterial);
}
static remixapi_MaterialHandle CreateSurfaceMaterialOwned(const remixapi_MaterialInfo& info,const SurfaceResourceOperation& operation) {
  return surfaceResources.CreateMaterialOwned(info,operation.scope);
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
  {remixapi_MaterialHandle cached=nullptr;if(surfaceResources.FindMaterialOwned(hash,operation.scope,cached))return cached;}
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
  }catch(const std::bad_alloc&){surfaceResources.RecordFailure();return nullptr;}
}

template<class Current>
static remixapi_MaterialHandle SurfaceChannelMaterialCurrent(IDirect3DDevice9* d,uint64_t textureHash,const material_channels::Plan& plan,
                                                       const native_material_source::Sampler* nativeSampler,
                                                       IDirect3DBaseTexture9* textureSource,const DWORD* sourceSrgb,const Current& current,
                                                       material_channels::TextureMipSource::Snapshot* snapshot=nullptr,
                                                       const material_channels::TextureMipSource::Description* expected=nullptr) {
  try {
  SurfaceResourceOperation operation;if(!operation.owned||!current())return nullptr;
  DWORD u=0,v=0,mag=0,srgb=0;
  if(nativeSampler){u=nativeSampler->u;v=nativeSampler->v;mag=nativeSampler->mag;}
  else if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u))||FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v))||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag)))return nullptr;
  if(sourceSrgb)srgb=*sourceSrgb;
  else if(FAILED(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb)))return nullptr;
  if(!current()||srgb||
     u<1||u>3||v<1||v>3||mag<1||mag>3||!surfaceAssetDirectory[0])return nullptr;
  const auto hash=SurfaceChannelMaterialIdentity(textureHash,u,v,mag,plan);
  if(!hash)return nullptr;
  {remixapi_MaterialHandle cached=nullptr;if(surfaceResources.FindMaterialOwned(hash,operation.scope,cached))return cached;}
  wchar_t albedo[MAX_PATH]{},emission[MAX_PATH]{};
  swprintf_s(albedo,L"%s\\%016llX-albedo.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  swprintf_s(emission,L"%s\\%016llX-emission.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  const bool emissive=!material_channels::Zero(plan.emission);
  const auto write=[&](const wchar_t* path,const material_channels::RGB& tint){
    if(snapshot) {
      if(!current())return false;
      if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
      if(!snapshot->valid)*snapshot=textureSource?material_channels::CaptureTextureMipSnapshotCurrent(textureSource,current):
        material_channels::CaptureBoundTextureMipSnapshotCurrent(d,current);
      if(!snapshot->valid||!material_channels::TextureMipDescriptionMatches(*snapshot,expected)||
         material_channels::TextureMipSnapshotHash(*snapshot)!=textureHash) {
        if(textureSource)InvalidateSurfaceTexture(textureSource);
        return false;
      }
      return material_channels::WriteTextureMipSnapshotCurrent(*snapshot,path,tint,current);
    }
    return textureSource?material_channels::WriteTextureSourceCurrent(textureSource,path,tint,current):material_channels::WriteTextureCurrent(d,path,tint,current);
  };
  if(!write(albedo,plan.albedo)||(emissive&&!write(emission,plan.emission))||!current())return nullptr;
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=.5f;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;info.hash=hash;
  info.albedoTexture=albedo;info.emissiveTexture=emissive?emission:nullptr;info.emissiveIntensity=emissive?1.f:0.f;
  info.wrapModeU=static_cast<uint8_t>(u-1);info.wrapModeV=static_cast<uint8_t>(v-1);info.filterMode=mag==D3DTEXF_POINT?0:1;
  return CreateSurfaceMaterialOwned(info,operation);
  }catch(const std::bad_alloc&){surfaceResources.RecordFailure();return nullptr;}
}
static remixapi_MaterialHandle SurfaceChannelMaterial(IDirect3DDevice9* d,uint64_t textureHash,const material_channels::Plan& plan,
                                                       const native_material_source::Sampler* nativeSampler=nullptr,
                                                       IDirect3DBaseTexture9* textureSource=nullptr,const DWORD* sourceSrgb=nullptr) {
  return SurfaceChannelMaterialCurrent(d,textureHash,plan,nativeSampler,textureSource,sourceSrgb,[]{return true;});
}

// Existing key selection stays in the adapter; the registry owns API resources.
static remixapi_MeshHandle CreateSurfaceGeometryOwned(const std::vector<remixapi_HardcodedVertex>& vertices,
    const std::vector<uint32_t>& indices,remixapi_MaterialHandle material,uint64_t hash,
    const SurfaceResourceOperation& operation,const remixapi_MeshInfoSkinning* skinning=nullptr) {
  return surfaceResources.CreateGeometryOwned(vertices,indices,material,hash,operation.scope,skinning);
}
static remixapi_MeshHandle SurfaceIndexedGeometryResource(const std::vector<remixapi_HardcodedVertex>& vertices,
    const std::vector<uint32_t>& indices,remixapi_MaterialHandle material) {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  if(vertices.empty()||vertices.size()>65536||indices.empty()||indices.size()>32768*3||indices.size()%3)return nullptr;
  const auto vertexHash=XXH3_64bits_withSeed(vertices.data(),vertices.size()*sizeof(vertices[0]),reinterpret_cast<uintptr_t>(material));
  const auto hash=XXH3_64bits_withSeed(indices.data(),indices.size()*sizeof(indices[0]),vertexHash^0x534b494e494e4458ull);
  return CreateSurfaceGeometryOwned(vertices,indices,material,hash,operation);
}

// Stable API resource identity for expanded rigid geometry.
// Geometry content is separate from its immutable material binding. Native
// owner/motion keys stay in the instance adapter; legacy Skin helpers keep
// their existing identity contract until their renderer path is qualified.
static remixapi_MeshHandle SurfaceRigidGeometryResourceStable(const std::vector<remixapi_HardcodedVertex>& vertices,
                                                              remixapi_MaterialHandle material) {
  try {
    SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
    const auto api=GetRemixApi();
    if(!api||!api->CreateMesh||!api->DestroyMesh)return nullptr;
    uint64_t materialIdentity=0;
    if(!surfaceResources.MaterialIdentityOwned(material,operation.scope,materialIdentity))return nullptr;
    SurfaceIdentityHash digest;
    const auto geometry=winx_remix::ResourceIdentitySource::GeometryExpanded(vertices.data(),vertices.size(),digest);
    const auto key=winx_remix::ResourceIdentitySource::BoundMesh(geometry,materialIdentity,digest);
    if(!key)return nullptr;
    remixapi_MeshHandle cached=nullptr;
    if(surfaceResources.FindMeshOwned(key,operation.scope,cached,material))return cached;
    if(vertices.size()>winx_remix::RemixResourceRegistry::MeshByteLimit/(sizeof(vertices[0])+4))return nullptr;
    std::vector<uint32_t> indices(vertices.size());
    for(size_t i=0;i<indices.size();++i)indices[i]=static_cast<uint32_t>(i);
    return CreateSurfaceGeometryOwned(vertices,indices,material,key,operation);
  }catch(const std::bad_alloc&){surfaceResources.RecordFailure();return nullptr;}
}

// Legacy FFP overlays and ordinary independent/selected packets share the same
// content identity and live material owner checks. Neither padding nor a
// process-local material handle defines another asset or cache entry.
static remixapi_MeshHandle SurfaceGeometryResource(const std::vector<remixapi_HardcodedVertex>& expanded,
                                                  remixapi_MaterialHandle material) {
  return SurfaceRigidGeometryResourceStable(expanded,material);
}

#include "winx_skin_packet_submit.h"
#include "winx_skin_draw_plan.h"
#include "winx_skin_gpu_submit.h"

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
    surfaceResources.RecordFailure();return submitted;
  }
}
#include "winx_independent_scene_submit.h"
#include "winx_selected_scene_submit.h"
