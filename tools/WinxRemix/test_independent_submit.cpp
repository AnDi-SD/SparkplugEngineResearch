// Own CPU/recording boundary fixture. Literal ABI data and a minimal owned COM
// test double exercise the adapter; no game instructions, system D3D or GPU run.
#include <new>
#include <cstdlib>
static int failAllocationAfter=-1;
void* operator new(size_t size) {
  if(failAllocationAfter==0){failAllocationAfter=-1;throw std::bad_alloc();}
  if(failAllocationAfter>0)--failAllocationAfter;
  if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,size_t) noexcept{std::free(p);}
void operator delete[](void* p,size_t) noexcept{std::free(p);}
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>

namespace direct_test {
namespace source=independent_scene_source;
namespace transport=native_transport_source;
namespace abi=sparkplug::evidence::pc;
static unsigned checks,comCalls;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static uint32_t Ptr(const void* value){return uint32_t(reinterpret_cast<uintptr_t>(value));}
static void Put(void* data,unsigned offset,uint32_t value){memcpy(static_cast<uint8_t*>(data)+offset,&value,4);}
static void Vector(void* data,unsigned offset,uint32_t* values,unsigned count,unsigned capacity=0) {
  Put(data,offset+4,Ptr(values));Put(data,offset+8,Ptr(values)+4*count);Put(data,offset+12,Ptr(values)+4*(capacity?capacity:count));
}
struct Watchdog {
  HANDLE event=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* e){if(WaitForSingleObject(e,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0520c30u);return 0;}
  Watchdog(){event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"owned watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"owned watchdog thread");}
  ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
struct Fixture;
static Fixture* current;
static void (*onTarget)();
static void (*onTargetRelease)();
static void (*onTextureRead)();
static void (*onTextureUnlock)();
static void (*onTextureAddRef)();
static void (*onTextureRelease)();
static void (*onApi)(char,unsigned);
static unsigned materialCalls,meshCalls,drawCalls,failDrawAt,targetCalls,cameraCalls;
static unsigned textureReadCalls;
static bool failMaterial,failMesh,failCamera;
static uintptr_t nextHandle=0x1000;
static std::set<remixapi_MaterialHandle> liveMaterials;
static std::map<remixapi_MeshHandle,remixapi_MaterialHandle> liveMeshes;
static std::vector<remixapi_InstanceInfo> instances;
static std::vector<remixapi_InstanceInfoBlendEXT> blends;
static std::map<source::Key,unsigned> observedAttempts;
static std::vector<uint32_t> drawnModels;
static std::vector<remixapi_CameraInfo> cameras;
static std::vector<char> apiOrder;
static unsigned lightCreates,lightDraws,lightDestroys,lightPolicyCalls;
static bool failLightCreate,failLightDraw;
static std::set<remixapi_LightHandle> liveLights;
static std::vector<remixapi_Float3D> lightPositions;
static remixapi_ErrorCode REMIXAPI_CALL CreateSceneLight(const remixapi_LightInfo* info,remixapi_LightHandle* output) {
  ++lightCreates;apiOrder.push_back('i');Check(info&&output&&info->pNext,"owned scene light description");
  const auto sphere=static_cast<const remixapi_LightInfoSphereEXT*>(info->pNext);
  Check(sphere->sType==REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT,"offscreen point light uses explicit sphere API");
  lightPositions.push_back(sphere->position);
  *output=reinterpret_cast<remixapi_LightHandle>(++nextHandle);liveLights.insert(*output);
  if(onApi)onApi('i',lightCreates);
  return failLightCreate?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DrawSceneLight(remixapi_LightHandle handle) {
  ++lightDraws;apiOrder.push_back('l');Check(liveLights.count(handle)==1,"scene light has current API owner");
  if(onApi)onApi('l',lightDraws);
  return failLightDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroySceneLight(remixapi_LightHandle handle) {
  ++lightDestroys;Check(liveLights.erase(handle)==1,"scene light destroyed exactly once");
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL SceneLightPolicy(const char* key,const char* value) {
  ++lightPolicyCalls;apiOrder.push_back('p');Check(key&&value,"explicit light fallback policy");
  if(onApi)onApi('p',lightPolicyCalls);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL SetupCamera(const remixapi_CameraInfo* info) {
  ++cameraCalls;Check(info&&info->sType==REMIXAPI_STRUCT_TYPE_CAMERA_INFO&&info->type==REMIXAPI_CAMERA_TYPE_WORLD&&!info->pNext,
    "early WORLD camera receives the original native matrix payload");
  cameras.push_back(*info);apiOrder.push_back('c');if(onApi)onApi('c',cameraCalls);
  return failCamera?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMaterial(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* out) {
  ++materialCalls;Check(info&&info->albedoTexture&&out,"recording material receives owned synchronous description");
  if(failMaterial)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  *out=reinterpret_cast<remixapi_MaterialHandle>(++nextHandle);liveMaterials.insert(*out);
  if(onApi)onApi('a',materialCalls);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out) {
  ++meshCalls;Check(info&&out&&info->surfaces_count==1,"recording mesh has one expanded surface");
  const auto& s=info->surfaces_values[0];Check(s.vertices_count==3&&s.indices_count==3&&liveMaterials.count(s.material),"expanded triangle owns valid material");
  for(unsigned i=0;i<3;++i)Check(s.indices_values[i]==i&&std::isfinite(s.vertices_values[i].position[0]),"owned expanded payload survives texture COM reads");
  if(failMesh)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  *out=reinterpret_cast<remixapi_MeshHandle>(++nextHandle);liveMeshes.emplace(*out,s.material);
  if(onApi)onApi('m',meshCalls);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroyMesh(remixapi_MeshHandle mesh) {
  Check(liveMeshes.erase(mesh)==1,"recording mesh destroyed exactly once");return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroyMaterial(remixapi_MaterialHandle material) {
  for(const auto& mesh:liveMeshes)Check(mesh.second!=material,"material outlives each dependent mesh");
  Check(liveMaterials.erase(material)==1,"recording material destroyed exactly once");return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DrawInstance(const remixapi_InstanceInfo* info) {
  ++drawCalls;apiOrder.push_back('d');Check(info&&info->pNext&&liveMeshes.count(info->mesh),"draw uses live prepared mesh and local blend");
  const auto blend=static_cast<const remixapi_InstanceInfoBlendEXT*>(info->pNext);
  Check(blend->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT,"relocated PreparedDirect fixes pNext");
  instances.push_back(*info);blends.push_back(*blend);
  for(const auto& entry:source::directUses)if(entry.second.attempted>observedAttempts[entry.first]) {
    Check(entry.second.attempted==observedAttempts[entry.first]+1,"each occurrence advances exactly one recording ledger token");
    observedAttempts[entry.first]=entry.second.attempted;drawnModels.push_back(entry.first.second);
  }
  if(onApi)onApi('d',drawCalls);
  return drawCalls==failDrawAt?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
struct FakeSurface {void** table=nullptr;ULONG refs=1;};
struct FakeTexture {
  void** table=nullptr;DWORD pixel=0xff805020;
  ULONG refs=1;bool alive=true,nativeOwner=true,locked=false;
  unsigned addRefs=0,releases=0,locks=0,unlocks=0,destroyed=0;
};
struct FakeDevice {void** table=nullptr;};
static HRESULT STDMETHODCALLTYPE GetTarget(FakeDevice*,DWORD index,IDirect3DSurface9** out);
static HRESULT STDMETHODCALLTYPE GetState(FakeDevice*,D3DRENDERSTATETYPE type,DWORD* out);
static HRESULT STDMETHODCALLTYPE GetStage(FakeDevice*,DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD* out);
static HRESULT STDMETHODCALLTYPE GetSampler(FakeDevice*,DWORD stage,D3DSAMPLERSTATETYPE type,DWORD* out);
static HRESULT STDMETHODCALLTYPE GetTransform(FakeDevice*,D3DTRANSFORMSTATETYPE type,D3DMATRIX* out);
static ULONG STDMETHODCALLTYPE ReleaseTarget(FakeSurface* surface){++comCalls;const auto refs=--surface->refs;if(onTargetRelease)onTargetRelease();return refs;}
static void TextureAlive(FakeTexture* texture) {
  Check(texture&&texture->alive&&texture->refs,"texture method cannot inspect a destroyed owned COM object");
}
static ULONG STDMETHODCALLTYPE TextureAddRef(FakeTexture* texture) {
  ++comCalls;TextureAlive(texture);++texture->addRefs;const auto refs=++texture->refs;
  if(onTextureAddRef)onTextureAddRef();return refs;
}
static ULONG STDMETHODCALLTYPE TextureRelease(FakeTexture* texture) {
  ++comCalls;TextureAlive(texture);++texture->releases;const auto refs=--texture->refs;
  if(!refs) {
    Check(!texture->locked,"final texture destruction occurs after its readonly lock closes");
    texture->alive=false;++texture->destroyed;
    transport::ForgetTexture(reinterpret_cast<IDirect3DTexture9*>(texture));
    surfaceChannelTextureHashes.erase(reinterpret_cast<IDirect3DBaseTexture9*>(texture));
    surfaceTextureHashes.erase(reinterpret_cast<IDirect3DBaseTexture9*>(texture));
  }
  if(onTextureRelease)onTextureRelease();return refs;
}
static D3DRESOURCETYPE STDMETHODCALLTYPE TextureType(FakeTexture* texture){++comCalls;TextureAlive(texture);return D3DRTYPE_TEXTURE;}
static DWORD STDMETHODCALLTYPE TextureLevels(FakeTexture* texture){++comCalls;TextureAlive(texture);return 1;}
static HRESULT STDMETHODCALLTYPE TextureDesc(FakeTexture* texture,UINT level,D3DSURFACE_DESC* out) {
  ++comCalls;TextureAlive(texture);if(level||!out)return D3DERR_INVALIDCALL;*out={};out->Format=D3DFMT_A8R8G8B8;out->Type=D3DRTYPE_SURFACE;
  out->Pool=D3DPOOL_MANAGED;out->Width=out->Height=1;return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE TextureLock(FakeTexture* texture,UINT level,D3DLOCKED_RECT* out,const RECT*,DWORD flags) {
  ++comCalls;TextureAlive(texture);++textureReadCalls;Check(!level&&flags==D3DLOCK_READONLY&&!texture->locked,"direct hash/DDS calls are exclusively readonly on qualified source");
  texture->locked=true;++texture->locks;
  out->pBits=&texture->pixel;out->Pitch=4;transport::TextureLockResult(reinterpret_cast<IDirect3DTexture9*>(texture),0,flags,D3D_OK);
  if(onTextureRead)onTextureRead();return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE TextureUnlock(FakeTexture* texture,UINT level) {
  ++comCalls;TextureAlive(texture);Check(!level&&texture->locked,"owned texture closes matching mip");
  texture->locked=false;++texture->unlocks;
  transport::TextureUnlockResult(reinterpret_cast<IDirect3DTexture9*>(texture),0,D3D_OK);
  if(onTextureUnlock)onTextureUnlock();return D3D_OK;
}
struct Fixture {
  uint32_t engineSlot=0,rendererSlot=0,managerSlot=0,rootSlot=0,queueMode=0;
  uint32_t textureBindingDebugSlot=0;
  unsigned char textureBindingDebug[0x20]{};
  unsigned char managerCode[7]{};
  alignas(4) uint8_t engine[0x80]{},renderer[0xf368]{},system[0x1d8]{},partitionRoot[0x84]{},object[0x10c]{},selection[0x3c]{};
  abi::spSceneLayout scene{};abi::spSceneManagerLayout manager{};abi::spNodeLayout systemRoot{};
  abi::spCameraObservedLayout camera{};
  abi::spModelLayout models[3]{};
  abi::spDXMaterialObservedLayout material{},defaultMaterial{};
  abi::spMaterialPassLayerObservedLayout pass{},defaultPass{};
  abi::spStdLayerObservedLayout layer{},defaultLayer{};
  abi::spMaterialTextureObservedLayout texture{},defaultTexture{};
  abi::spDXTextureObservedLayout nativeTexture{};
  abi::spDXMeshObservedLayout mesh{};abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  uint32_t declaration[7]{},bufferTokens[3]{},renderables[5]{},originalSelection[1]{};
  D3DMATRIX world{},deviceView{},deviceProjection{};D3DVERTEXELEMENT9 layout[5]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
    {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
    {0,28,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
  void* deviceTable[119]{};void* textureTable[21]{};void* targetTable[17]{};
  FakeDevice device{};FakeTexture comTexture{};FakeSurface target{};
  std::array<DWORD,256> states{};std::array<DWORD,33> stages{};
  remixapi_Interface api{};
  alignas(4) unsigned char sceneLightManager[0x24]{},sceneLight[0x158]{};
  IDirect3DDevice9* Device(){return reinterpret_cast<IDirect3DDevice9*>(&device);}
  IDirect3DTexture9* Texture(){return reinterpret_cast<IDirect3DTexture9*>(&comTexture);}
  uint32_t Support(){return Ptr(object)+0x14;}
  Fixture() {
    textureBindingDebugSlot=Ptr(textureBindingDebug);
    source::textureBindingDebugPointerAddress=reinterpret_cast<uintptr_t>(&textureBindingDebugSlot);
    current=this;frameId=(std::max)(frameId,800u)+3;drawId=0;materialCalls=meshCalls=drawCalls=failDrawAt=targetCalls=cameraCalls=textureReadCalls=0;failMaterial=failMesh=failCamera=false;onApi=nullptr;onTarget=nullptr;onTargetRelease=nullptr;onTextureRead=nullptr;
    onTextureUnlock=onTextureAddRef=onTextureRelease=nullptr;
    source::submissionFailed=false;source::submitEnabled=true;source::keepForComparison=false;source::BeginDirectScope();source::inputs.clear();
    source::queueModeAddress=reinterpret_cast<uintptr_t>(&queueMode);
    source::directGroups=source::directInstances=source::directRejected=source::directApiFailures=0;
    materialChannelsEnabled=true;keepMaterialChannelsForComparison=false;source::enabled=true;source::ownerThread=GetCurrentThreadId();
    opaqueAlphaTest=false;keepTrivialAlphaTestForComparison=false;
    sceneLightsEnabled=false;keepSceneLightsForComparison=false;
    lightCreates=lightDraws=lightDestroys=lightPolicyCalls=0;failLightCreate=failLightDraw=false;lightPositions.clear();
    native_owner_source::enabled=true;native_owner_source::ownerThread=GetCurrentThreadId();
    native_owner_source::enginePointerAddress=reinterpret_cast<uintptr_t>(&engineSlot);native_owner_source::rendererPointerAddress=reinterpret_cast<uintptr_t>(&rendererSlot);
    engineSlot=Ptr(engine);rendererSlot=Ptr(renderer);Put(engine,0x18,Ptr(&scene));Put(engine,0x1c,Ptr(&camera));
    Put(renderer,0,abi::spPCRendererPrimaryVTable);Put(renderer,0xc9e8,Ptr(Device()));
    Put(&scene,0,0x6e7358);scene.systemRoot=Ptr(&systemRoot);scene.partitionSystem=Ptr(system);Put(system,0x1d4,Ptr(partitionRoot));
    Put(&systemRoot,0,abi::spNodeVTable);systemRoot.sceneLink=Ptr(&scene);
    Put(&manager,0,0x6e7154);managerSlot=Ptr(&manager);
    native_update_source::managerPointerAddress=reinterpret_cast<uintptr_t>(&managerSlot);
    native_update_source::managerEntry=reinterpret_cast<uintptr_t>(managerCode);
    memcpy(managerCode,native_update_source::managerReplacement,7);
    rootSlot=Ptr(reinterpret_cast<void*>(&native_update_source::RootUpdate));native_update_source::rootSlotAddress=reinterpret_cast<uintptr_t>(&rootSlot);
    native_update_source::enabled=true;native_update_source::ownerThread=GetCurrentThreadId();
    Put(&camera,0,0x6ef1e0);camera.viewMatrix[0]=camera.viewMatrix[5]=camera.viewMatrix[10]=camera.viewMatrix[15]=1;
    camera.viewMatrix[1]=-0.0f;camera.viewMatrix[12]=-13.25f;camera.projectionMatrix[0]=1.375f;camera.projectionMatrix[5]=1.625f;
    camera.projectionMatrix[10]=1.001f;camera.projectionMatrix[11]=1;camera.projectionMatrix[14]=-.125f;
    memcpy(&deviceView,camera.viewMatrix,64);memcpy(&deviceProjection,camera.projectionMatrix,64);
    native_camera_source::enabled=native_camera_source::submitEnabled=true;
    native_camera_source::ownerThread=GetCurrentThreadId();
    native_camera_source::observedFrame=native_camera_source::attemptedFrame=native_camera_source::submittedFrame=frameId;
    auto& accepted=native_camera_source::selected;accepted={};accepted.valid=true;accepted.frame=frameId;accepted.scene=Ptr(&scene);accepted.camera=Ptr(&camera);
    accepted.sequence=100;accepted.deviceEpoch=native_camera_source::deviceEpoch;accepted.info.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;accepted.info.type=REMIXAPI_CAMERA_TYPE_WORLD;
    memcpy(accepted.info.view,camera.viewMatrix,64);memcpy(accepted.info.projection,camera.projectionMatrix,64);native_camera_source::pending=accepted;
    device.table=deviceTable;deviceTable[38]=reinterpret_cast<void*>(GetTarget);deviceTable[58]=reinterpret_cast<void*>(GetState);
    deviceTable[66]=reinterpret_cast<void*>(GetStage);deviceTable[68]=reinterpret_cast<void*>(GetSampler);
    deviceTable[45]=reinterpret_cast<void*>(GetTransform);
    deviceTable[81]=reinterpret_cast<void*>(Draw);deviceTable[82]=reinterpret_cast<void*>(DrawIndexed);
    deviceTable[83]=reinterpret_cast<void*>(DrawUP);deviceTable[84]=reinterpret_cast<void*>(DrawIndexedUP);
    target.table=targetTable;targetTable[2]=reinterpret_cast<void*>(ReleaseTarget);primaryTargets.emplace(Device(),reinterpret_cast<IDirect3DSurface9*>(&target));
    comTexture.table=textureTable;textureTable[1]=reinterpret_cast<void*>(TextureAddRef);textureTable[2]=reinterpret_cast<void*>(TextureRelease);
    textureTable[10]=reinterpret_cast<void*>(TextureType);textureTable[13]=reinterpret_cast<void*>(TextureLevels);
    textureTable[17]=reinterpret_cast<void*>(TextureDesc);textureTable[19]=reinterpret_cast<void*>(TextureLock);textureTable[20]=reinterpret_cast<void*>(TextureUnlock);
    states[D3DRS_ALPHAFUNC]=D3DCMP_ALWAYS;states[D3DRS_COLORWRITEENABLE]=15;states[D3DRS_BLENDOP]=D3DBLENDOP_ADD;
    stages[D3DTSS_COLORARG1]=stages[D3DTSS_ALPHAARG1]=D3DTA_TEXTURE;stages[D3DTSS_COLORARG2]=stages[D3DTSS_ALPHAARG2]=stages[D3DTSS_RESULTARG]=D3DTA_CURRENT;
    Put(object,0,0x6e65e8);Put(object,0x14,0x6e6604);Put(object,0x84,Ptr(object));Put(object,0x88,Ptr(&scene));
    Put(object,0x48,Ptr(object)+0x8c);Put(object,0x4c,Ptr(object)+0xcc);world._11=world._22=world._33=world._44=1;world._41=12;
    memcpy(object+0x8c,&world,64);memcpy(object+0xcc,&world,64);memcpy(renderer+0xca40,&world,64);
    for(auto& m:models){m.base.base.base.vtableAddress=0x6eaa58;m.base.material=Ptr(&material);m.baseMeshData=Ptr(&mesh);}
    renderables[0]=Ptr(&models[0]);renderables[1]=0;renderables[2]=Ptr(&models[1]);renderables[3]=Ptr(&models[0]);Vector(object,0x18,renderables,4,5);
    Vector(selection,0x28,originalSelection,0,1);
    material.base.base.vtableAddress=abi::spDXMaterialPrimaryVTable;material.base.materialVTable=abi::spDXMaterialInterfaceVTable;
    material.base.passCount=1;material.base.passes[0]=Ptr(&pass);material.base.renderStates[8]=2;
    pass.base.vtableAddress=abi::spMaterialPassLayerVTable;pass.layerCount=1;pass.layers[0]=Ptr(&layer);
    layer.base.base.vtableAddress=abi::spStdLayerVTable;layer.base.materialTexture=Ptr(&texture);
    texture.base.vtableAddress=abi::spMaterialTextureVTable;texture.fallbackTexture=Ptr(&nativeTexture);
    const uint32_t raw[9]={0,3,3,0,0,2,2,0,0};memcpy(texture.textureStates,raw,sizeof(raw));
    nativeTexture.base.base.base.base.vtableAddress=abi::spDXTexturePrimaryVTable;nativeTexture.device=Ptr(Device());nativeTexture.texture=Ptr(Texture());
    defaultMaterial=material;defaultMaterial.base.passes[0]=Ptr(&defaultPass);defaultPass=pass;defaultPass.layers[1]=Ptr(&defaultLayer);
    defaultLayer=layer;defaultLayer.base.materialTexture=Ptr(&defaultTexture);defaultTexture=texture;defaultTexture.textureStates[1]=0;
    Put(renderer,0xc9c0,Ptr(&defaultMaterial));
    mesh.base.base.base.base.base.vtableAddress=abi::spDXMeshVTable;mesh.base.base.secondaryVTable=abi::spDXMeshInterfaceVTable;
    mesh.base.base.vertexCount=3;mesh.base.base.primitiveCount=1;mesh.base.base.vertexComponentFlags=0x940;mesh.indexType=2;mesh.vertexStride=36;mesh.fvfCode=0x152;
    mesh.vertexBuffer=Ptr(&vb);mesh.indexBuffer=Ptr(&ib);mesh.vertexDeclaration=Ptr(declaration);
    vb.base.vtableAddress=abi::spDXVertexBufferVTable;vb.direct3DVertexBuffer=Ptr(&bufferTokens[0]);vb.byteSize=108;
    ib.base.vtableAddress=abi::spDXIndexBufferVTable;ib.direct3DIndexBuffer=Ptr(&bufferTokens[1]);ib.byteSize=6;
    declaration[0]=0x6f2e58;declaration[5]=mesh.fvfCode;declaration[6]=Ptr(&bufferTokens[2]);
    struct Vertex {float p[3],n[3];DWORD color;float uv[2];};
    const Vertex vertices[]={{{0,0,0},{0,0,1},0xffffffff,{0,0}},{{1,0,0},{0,0,1},0xffffffff,{1,0}},{{0,1,0},{0,0,1},0xffffffff,{0,1}}};const uint16_t indices[]={0,1,2};
    std::vector<uint8_t> v(sizeof(vertices)),i(sizeof(indices));memcpy(v.data(),vertices,v.size());memcpy(i.data(),indices,i.size());
    native_mesh_source::enabled=true;native_mesh_source::ownerThread=GetCurrentThreadId();
    native_mesh_source::buffers.emplace(&bufferTokens[0],native_mesh_source::Bytes{v,17,0,Ptr(&vb)});
    native_mesh_source::buffers.emplace(&bufferTokens[1],native_mesh_source::Bytes{i,17,0,Ptr(&ib)});native_mesh_source::retainedBytes=v.size()+i.size();
    surfaceBuffers.emplace(&bufferTokens[0],SurfaceBuffer{v,true});surfaceBuffers.emplace(&bufferTokens[1],SurfaceBuffer{i,true});
    transport::enabled=true;transport::Remember(Device(),&bufferTokens[0],transport::Kind::Vertex,vb.byteSize,D3DFMT_UNKNOWN);
    transport::Remember(Device(),&bufferTokens[1],transport::Kind::Index,ib.byteSize,D3DFMT_INDEX16);
    transport::Remember(Device(),&bufferTokens[2],transport::Kind::Declaration,0,D3DFMT_UNKNOWN,layout,5);
    D3DSURFACE_DESC desc{};TextureDesc(&comTexture,0,&desc);Check(transport::RememberTexture(Device(),Texture(),Texture(),&desc,1),"owned texture creation metadata qualifies");
    api.CreateMaterial=CreateMaterial;api.CreateMesh=CreateMesh;api.DrawInstance=DrawInstance;api.DestroyMesh=DestroyMesh;api.DestroyMaterial=DestroyMaterial;api.SetupCamera=SetupCamera;testRemixApi=&api;
    GetCurrentDirectoryW(MAX_PATH,surfaceAssetDirectory);wcscat_s(surfaceAssetDirectory,L"\\assets");CreateDirectoryW(surfaceAssetDirectory,nullptr);
    scene_geometry::skipExtendedSupport=source::DirectSupportOmitted;scene_geometry::selectionRestarted=source::DirectSelectionRestarted;
  }
  ~Fixture() {
    failAllocationAfter=-1;onApi=nullptr;onTarget=nullptr;onTargetRelease=nullptr;onTextureRead=nullptr;
    onTextureUnlock=onTextureAddRef=onTextureRelease=nullptr;
    Check(comTexture.refs==(comTexture.nativeOwner?1u:0u)&&comTexture.alive==comTexture.nativeOwner&&
      comTexture.locks==comTexture.unlocks&&!comTexture.locked,
      "every texture capture closes its lock and temporary reference before fixture retirement");
    if(sceneLightsEnabled){++frameId;ResetSceneLights();}
    sceneLightsEnabled=false;keepSceneLightsForComparison=false;
    Check(liveLights.empty(),"fixture retires all scene light handles");
    RetireSurfaceResources(true);testRemixApi=nullptr;
    source::inputs.clear();source::BeginDirectScope();native_mesh_source::buffers.clear();native_mesh_source::layouts.clear();native_mesh_source::retainedBytes=0;
    transport::records.clear();transport::declarationCount=0;transport::textures.clear();transport::textureSurfaces.clear();transport::resetting.clear();
    surfaceBuffers.clear();surfaceChannelTextureHashes.clear();surfaceTextureHashes.clear();primaryTargets.clear();instances.clear();blends.clear();observedAttempts.clear();drawnModels.clear();cameras.clear();apiOrder.clear();
    native_mesh_source::active=nullptr;native_owner_source::activeModel=nullptr;native_owner_source::activeSupport=nullptr;current=nullptr;
  }
  void Inputs(scene_geometry::Scope& scope,bool selected=false) {
    source::inputs.clear();source::BeginDirectScope();source::inputScope=scope.serial;source::inputSelectionManager=Ptr(selection);std::unique_lock<std::recursive_mutex> lock(guard);
    for(unsigned ordinal=0;ordinal<2;++ordinal) {
      source::Input in{};auto& o=in.owner;o.valid=true;o.scene=Ptr(&scene);o.system=Ptr(system);o.root=Ptr(partitionRoot);o.camera=Ptr(&camera);
      o.support=Support();o.object=Ptr(object);o.ownerPrimary=0x6e65e8;o.renderer=Ptr(renderer);o.frame=frameId;o.sceneScope=scope.serial;o.mutation=scene_geometry::MutationSerial();
      o.model=Ptr(&models[ordinal]);o.mesh=Ptr(&mesh);o.modelMaterial=Ptr(&material);o.modelOccurrences=ordinal?1:2;o.firstModelOrdinal=ordinal?2:0;o.registrations=1;
      in.witness={Ptr(&manager),Ptr(&scene),Ptr(&systemRoot),Ptr(system),Ptr(partitionRoot),frameId,native_update_source::Serial(),o.mutation,native_camera_source::deviceEpoch,GetCurrentThreadId(),true};
      abi::spRenderSupportObservedLayout support{};Check(source::Read(Support(),support)&&source::WorldInput(in,support),"owned world input qualifies");
      Check(source::MaterialInput(in,models[ordinal],Ptr(Device()),0),"owned ordinary native material maps");
      source::DrawBootstrap bootstrap{};Check(source::ReadDrawBootstrap(Device(),Ptr(renderer),bootstrap)&&source::DrawInput(bootstrap,in),"owned current bootstrap maps");
      Check(source::ResourceInput(lock,Device(),in),"owned current mesh CPU/transport sources qualify");
      Check(transport::BorrowTexture(lock,Device(),Texture(),in.textureWitness),"owned texture token borrowed");
      in.originallySelected=selected;source::inputs.emplace(source::Key{Support(),Ptr(&models[ordinal])},in);
    }
  }
  void EarlyCamera() {
    native_camera_source::Reset();drawId=0;
    Check(native_camera_source::Capture(Ptr(&scene),Ptr(&camera),Ptr(&camera),0x6ef1e0,camera),"owned early apply uses production Capture before independent packet");
  }
  void SceneLights(bool enabled=true) {
    api.CreateLight=CreateSceneLight;api.DrawLightInstance=DrawSceneLight;
    api.DestroyLight=DestroySceneLight;api.SetConfigVariable=SceneLightPolicy;
    Put(&scene,0x34,Ptr(sceneLightManager));
    Put(sceneLightManager,0,0x6e8ca4);Put(sceneLightManager,0x20,Ptr(&scene));
    Put(sceneLightManager,0x10,Ptr(sceneLight));Put(sceneLightManager,0x14,Ptr(sceneLight));Put(sceneLightManager,0x18,1);
    Put(sceneLight,0,0x6f0c88);Put(sceneLight,0x3c,Ptr(&scene));Put(sceneLight,0xb0,0x100);Put(sceneLight,0xc0,1);
    sceneLight[0xed]=1;
    D3DLIGHT9 payload{};payload.Type=D3DLIGHT_POINT;payload.Diffuse={1,.5f,.25f,1};
    payload.Position={-100,0,-100};payload.Range=1000;payload.Attenuation0=1;
    memcpy(sceneLight+0xf0,&payload,sizeof(payload));sceneLightsEnabled=enabled;
  }
  void ChangeSceneLight() {
    float changed=2;memcpy(sceneLight+0xf4,&changed,4);
  }
  void Submit(scene_geometry::Scope& scope){std::unique_lock<std::recursive_mutex> lock(guard);source::SubmitCurrent(lock,scope);}
};
static HRESULT STDMETHODCALLTYPE GetTarget(FakeDevice*,DWORD index,IDirect3DSurface9** out) {
  ++comCalls;++targetCalls;Check(index==0&&out,"only current primary target requested");if(onTarget)onTarget();++current->target.refs;*out=reinterpret_cast<IDirect3DSurface9*>(&current->target);return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE GetState(FakeDevice*,D3DRENDERSTATETYPE type,DWORD* out){++comCalls;if(unsigned(type)>=current->states.size())return D3DERR_INVALIDCALL;*out=current->states[type];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetStage(FakeDevice*,DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD* out){++comCalls;if(stage==1&&type==D3DTSS_COLOROP){*out=D3DTOP_DISABLE;return D3D_OK;}if(stage||unsigned(type)>=current->stages.size())return D3DERR_INVALIDCALL;*out=current->stages[type];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetSampler(FakeDevice*,DWORD stage,D3DSAMPLERSTATETYPE type,DWORD* out){++comCalls;if(stage||type!=D3DSAMP_SRGBTEXTURE)return D3DERR_INVALIDCALL;*out=0;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetTransform(FakeDevice*,D3DTRANSFORMSTATETYPE type,D3DMATRIX* out) {
  ++comCalls;if(type==D3DTS_VIEW)*out=current->deviceView;else if(type==D3DTS_PROJECTION)*out=current->deviceProjection;else return D3DERR_INVALIDCALL;return D3D_OK;
}

static void Basic() {
  Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
  const auto objectBefore=std::vector<uint8_t>(f.object,f.object+sizeof(f.object));f.Submit(scope);
  Check(drawCalls==3&&source::directInstances==3&&source::directGroups==1,"complete group preserves two references to first Model plus one to second");
  Check(drawnModels==std::vector<uint32_t>({Ptr(&f.models[0]),Ptr(&f.models[1]),Ptr(&f.models[0])}),"API order preserves native vector order with duplicate and null gap");
  Check(materialCalls==1&&meshCalls==1,"all group preparation shares one immutable resource cache");
  Check(textureReadCalls==1&&f.comTexture.locks==1&&f.comTexture.unlocks==1&&
    f.comTexture.refs==1&&f.comTexture.addRefs==f.comTexture.releases,
    "hash and DDS share one owned mip copy while later prepared Models reuse warm resources");
  Check(source::DirectSupportOmitted(f.Support()),"only fully transferred support eligible for omission");
  Check(std::equal(objectBefore.begin(),objectBefore.end(),f.object),"direct preparation never mutates support or native matrices");
  for(const auto& instance:instances)Check(instance.transform.matrix[0][3]==12,"direct world transform reaches actual recording API");
  scene_geometry::Registry registry{};registry.valid=true;registry.mutationSerial=scene_geometry::MutationSerial();registry.supports={f.Support()};
  scope.ExtendSelection(Ptr(f.selection),registry);
  Check(scope.marks.empty()&&!scope.manager&&scene_geometry::Word(f.Support()+0x64)==0,"omitted support receives no adapter visibility stamp");
  Check(f.target.refs==1,"temporary primary-target references balanced");
}
static void Rejections() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope,true);f.Submit(scope);
    Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"originally selected support retains original producer");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);source::inputs.erase({f.Support(),Ptr(&f.models[1])});f.Submit(scope);
    Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"one unsupported nonnull model rejects whole support");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);Put(f.renderer,0x4c,1);f.Submit(scope);
    Check(!drawCalls&&!materialCalls,"preexisting alpha queue blocks before preparation");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);Put(f.renderer,0xc058,Ptr(f.renderables));Put(f.renderer,0xc05c,Ptr(f.renderables)+20);Put(f.renderer,0xc060,Ptr(f.renderables)+20);f.Submit(scope);
    Check(!drawCalls&&!materialCalls,"nonempty general queue blocks before preparation");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);native_camera_source::submittedFrame=frameId-1;f.Submit(scope);
    Check(!drawCalls&&!materialCalls,"previous-frame camera is insufficient");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);f.camera.viewMatrix[12]=3;f.Submit(scope);
    Check(!drawCalls&&!materialCalls,"changed native view blocks before preparation");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);transport::TextureLockResult(f.Texture(),0,0,D3D_OK);transport::TextureUnlockResult(f.Texture(),0,D3D_OK);f.Submit(scope);
    Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"changed texture content rejects fresh group");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);failMaterial=true;f.Submit(scope);
    Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"material creation failure never omits support");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);failMesh=true;f.Submit(scope);
    Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"mesh creation failure never omits support");}
}
static void PartialFailure() {
  Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);failDrawAt=2;f.Submit(scope);
  Check(drawCalls==2&&source::directInstances==1&&source::submissionFailed,"partial API error stops remaining group calls");
  Check(source::DirectSupportOmitted(f.Support()),"current partial group quarantined against duplicate legacy draw");
  f.Submit(scope);Check(drawCalls==2,"failed submission disables another attempt");
  source::BeginDirectScope();Check(!source::DirectSupportOmitted(f.Support()),"next scope releases old omission ledger for fallback");
}
static void ChangedContent() {
  transport::TextureLockResult(current->Texture(),0,0,D3D_OK);
  transport::TextureUnlockResult(current->Texture(),0,D3D_OK);
}
static void ReleaseNativeTexture() {
  auto& texture=current->comTexture;
  Check(texture.nativeOwner&&texture.refs,"literal native owner holds one live reference before its release");
  texture.nativeOwner=false;const auto expected=texture.refs-1;
  Check(current->Texture()->Release()==expected,"native owner releases exactly its own texture reference");
}
// All mutations use owned literal records or the existing transport registry;
// this helper never calls a native producer or COM method.
static void ChangePacketSource(Fixture& fixture,unsigned reason) {
  switch(reason) {
  case 0:fixture.models[0].base.material=Ptr(&fixture.defaultMaterial);break;
  case 1:fixture.material.base.passes[0]=Ptr(&fixture.defaultPass);break;
  case 2:transport::TextureLockResult(fixture.Texture(),0,0,D3D_OK);break;
  case 3:transport::ForgetTexture(fixture.Texture());break;
  case 4:{D3DSURFACE_DESC description{};description.Format=D3DFMT_A8R8G8B8;description.Type=D3DRTYPE_SURFACE;
    description.Pool=D3DPOOL_MANAGED;description.Width=description.Height=1;
    Check(transport::RememberTexture(fixture.Device(),fixture.Texture(),fixture.Texture(),&description,1),
      "same-address texture replacement establishes a fresh owned creation record");break;}
  case 5:transport::BeginReset(fixture.Device());transport::EndReset(fixture.Device());break;
  case 6:fixture.textureBindingDebug[0x1f]=0x80;break;
  }
}
static void PacketSourceTransitions() {
  for(unsigned reason=0;reason<7;++reason) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    const auto input=source::inputs.at({f.Support(),Ptr(&f.models[0])});
    {std::unique_lock<std::recursive_mutex> borrow(guard);const auto before=comCalls;
      Check(source::NativeInputCurrent(borrow,f.Device(),input),"owned packet has a qualified native source before mutation");
      ChangePacketSource(f,reason);
      Check(!source::NativeInputCurrent(borrow,f.Device(),input)&&comCalls==before,
        "pure consumption fence rejects replaced material pass writable retired reused reset or NULL texture without COM");}
    f.Submit(scope);
    Check(!materialCalls&&!meshCalls&&!drawCalls&&!source::DirectSupportOmitted(f.Support())&&!source::submissionFailed,
      "stale owned packet preserves native direct fallback before any API resource ownership");
  }
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    const auto input=source::inputs.at({f.Support(),Ptr(&f.models[0])});
    const auto before=comCalls;std::unique_lock<std::recursive_mutex> borrow(guard);
    transport::TextureLockResult(f.Texture(),0,D3DLOCK_READONLY,D3D_OK);
    Check(source::NativeInputCurrent(borrow,f.Device(),input),"readonly source access preserves the captured content token");
    transport::TextureUnlockResult(f.Texture(),0,D3D_OK);
    Check(source::NativeInputCurrent(borrow,f.Device(),input)&&comCalls==before,
      "closing readonly source access preserves the pure material and texture fence");}
  const unsigned changes[]={0,1,3,4};
  for(unsigned reason:changes)for(unsigned boundary=0;boundary<3;++boundary) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned change,hits;static char operation;change=reason;hits=0;operation=boundary==0?'a':boundary==1?'m':'d';
    onApi=[](char op,unsigned ordinal){if(op==operation&&(op!='d'||ordinal==1)){++hits;ChangePacketSource(*current,change);}};
    f.Submit(scope);
    Check(hits==1&&materialCalls==1&&meshCalls==(boundary?1u:0u),
      "material pass retirement or same-address reuse happens at the requested resource or instance boundary");
    if(boundary<2)Check(!drawCalls&&!source::DirectSupportOmitted(f.Support())&&!source::submissionFailed,
      "late material or texture identity change preserves direct fallback before first instance");
    else Check(drawCalls==1&&source::directInstances==1&&source::submissionFailed&&source::DirectSupportOmitted(f.Support()),
      "identity change after accepted direct instance stops later occurrences and preserves current ownership");
  }
}
static void TextureReadReentry() {
  Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
  onTextureRead=[](){onTextureRead=nullptr;current->comTexture.pixel=0xff1234ab;ChangedContent();};
  f.Submit(scope);
  Check(textureReadCalls==1&&!materialCalls&&!meshCalls&&!drawCalls&&!source::DirectSupportOmitted(f.Support()),
    "content write during readonly hash COM call rejects before API resource or direct instance ownership");
  Check(!surfaceChannelTextureHashes.count(f.Texture()),"reentrant content mutation cannot publish a stale channel texture hash");
  const auto reads=textureReadCalls;f.Inputs(scope);f.Submit(scope);
  Check(textureReadCalls>reads&&drawCalls==3&&surfaceChannelTextureHashes.count(f.Texture())==1,
    "fresh operation rehashes current content and submits all repeated Model occurrences");
}
static void TextureLifetimeTransitions() {
  for(unsigned boundary=0;boundary<3;++boundary) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned hits;hits=0;const auto before=material_channels::assetBytes;
    const auto release=[](){
      onTextureRead=onTextureUnlock=onTextureRelease=nullptr;++hits;
      Check(current->comTexture.refs==2&&current->comTexture.alive,
        "owned mip capture holds its temporary reference before native owner release");
      ReleaseNativeTexture();
      Check(current->comTexture.refs==1&&current->comTexture.alive&&
        transport::textures.count(current->Texture())==1,
        "native owner release cannot retire or destroy texture while snapshot reference remains");
    };
    if(boundary==0)onTextureRead=release;
    else if(boundary==1)onTextureUnlock=release;
    else onTextureRead=[](){onTextureRead=nullptr;onTextureRelease=[](){
      if(current->comTexture.refs!=1)return;onTextureRelease=nullptr;++hits;ReleaseNativeTexture();
    };};
    f.Submit(scope);
    Check(hits==1&&!f.comTexture.refs&&!f.comTexture.alive&&f.comTexture.destroyed==1&&
      f.comTexture.locks==1&&f.comTexture.unlocks==1&&!f.comTexture.locked,
      "last native owner release at read unlock or temporary-release boundary closes one mip before final destruction");
    Check(!materialCalls&&!meshCalls&&!drawCalls&&!source::DirectSupportOmitted(f.Support())&&
      !source::submissionFailed&&!source::directRunning&&material_channels::assetBytes==before&&
      !surfaceChannelTextureHashes.count(f.Texture()),
      "destruction before final current fence publishes no hash DDS API resource or direct ownership");
  }
  for(unsigned boundary=0;boundary<3;++boundary)for(unsigned change=0;change<2;++change) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned mutation,hits,at;mutation=change;hits=0;at=boundary;
    const auto mutate=[](){onTextureRead=onTextureUnlock=onTextureRelease=nullptr;++hits;
      Check(current->comTexture.refs==(at==2?1u:2u)&&current->comTexture.alive,
        "content or Reset callback reaches its requested temporary ownership boundary");
      if(mutation==1){transport::BeginReset(current->Device());transport::EndReset(current->Device());}
      else {current->comTexture.pixel=0xff7234ab;ChangedContent();}};
    if(boundary==0)onTextureRead=mutate;
    else if(boundary==1)onTextureUnlock=mutate;
    else {onTextureRelease=mutate;}
    f.Submit(scope);
    Check(hits==1&&f.comTexture.refs==1&&f.comTexture.alive&&f.comTexture.addRefs==f.comTexture.releases&&
      f.comTexture.locks==1&&f.comTexture.unlocks==1&&!materialCalls&&!meshCalls&&!drawCalls&&
      !source::DirectSupportOmitted(f.Support())&&!surfaceChannelTextureHashes.count(f.Texture()),
      "content write or Reset at read unlock or final temporary release rejects stale snapshot after balanced ownership");
    if(!change) {
      f.Inputs(scope);f.Submit(scope);
      Check(hits==1&&drawCalls==3&&source::DirectSupportOmitted(f.Support())&&
        f.comTexture.refs==1&&f.comTexture.alive&&f.comTexture.addRefs==f.comTexture.releases&&
        f.comTexture.locks==f.comTexture.unlocks&&surfaceChannelTextureHashes.count(f.Texture())==1,
        "fresh direct source after cancelled mip capture uses current content and submits each occurrence once");
    }
  }
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static scene_geometry::Scope* activeScope;activeScope=&scope;
    onTextureRead=[](){onTextureRead=nullptr;current->Submit(*activeScope);};
    f.Submit(scope);
    Check(!materialCalls&&!meshCalls&&!drawCalls&&!source::DirectSupportOmitted(f.Support())&&!source::directRunning&&
      f.comTexture.refs==1&&f.comTexture.locks==f.comTexture.unlocks,
      "direct submission reentry inside mip lock cannot create API ownership and closes all temporary resources");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char op,unsigned ordinal){if(op=='d'&&ordinal==1)ReleaseNativeTexture();};
    f.Submit(scope);
    Check(drawCalls==1&&source::directInstances==1&&source::submissionFailed&&source::DirectSupportOmitted(f.Support())&&
      !f.comTexture.alive&&f.comTexture.destroyed==1,
      "native texture destruction after accepted instance retains direct ownership and stops remaining occurrences");
    f.Submit(scope);Check(drawCalls==1,"destroyed postcommit texture cannot replay direct instances");
    source::BeginDirectScope();Check(!source::DirectSupportOmitted(f.Support()),"new scope releases destroyed-texture omission for original fallback");}
}
static void TextureBindingTransitions() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    f.textureBindingDebug[0x1f]=1;f.Submit(scope);
    Check(!materialCalls&&!meshCalls&&!drawCalls&&!source::DirectSupportOmitted(f.Support())&&!source::submissionFailed,
      "forced NULL after capture preserves the original support before resource or instance creation");}
  for(unsigned boundary=0;boundary<2;++boundary) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static char operation;static unsigned hits;operation=boundary?'m':'a';hits=0;
    onApi=[](char op,unsigned){if(op==operation){++hits;current->textureBindingDebug[0x1f]=0x80;}};
    f.Submit(scope);
    Check(hits==1&&materialCalls==1&&meshCalls==boundary&&!drawCalls&&
      !source::DirectSupportOmitted(f.Support())&&!source::submissionFailed,
      "debug transition in material or mesh creation rejects once before direct instance ownership");
    Check(f.textureBindingDebug[0x1f]==0x80,"direct qualification never resets the native debug byte");
  }
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned hits;hits=0;
    onApi=[](char op,unsigned ordinal){if(op=='d'&&ordinal==1){++hits;current->textureBindingDebug[0x1f]=0xff;}};
    f.Submit(scope);
    Check(hits==1&&drawCalls==1&&source::directInstances==1&&!source::directApiFailures&&
      source::submissionFailed&&source::DirectSupportOmitted(f.Support()),
      "debug transition inside first accepted direct instance stops remaining models and retains current support ownership");
    f.textureBindingDebug[0x1f]=0;f.Submit(scope);
    Check(drawCalls==1&&source::DirectSupportOmitted(f.Support()),"clearing debug flag cannot replay a partially committed direct group");
    source::BeginDirectScope();
    Check(!source::DirectSupportOmitted(f.Support()),"new direct scope releases prior ownership for original fallback");
  }
}
static void SequenceAndReentry() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    f.originalSelection[0]=f.Support();Vector(f.selection,0x28,f.originalSelection,1);
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"actual original selection outranks a stale false packet flag");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    f.renderer[0xc050]=1;Put(f.renderer,0xc058,Ptr(f.renderables));Put(f.renderer,0xc05c,Ptr(f.renderables));Put(f.renderer,0xc060,Ptr(f.renderables)+20);
    f.Submit(scope);Check(drawCalls==3&&f.renderer[0xc050]==1,"future queue-routing byte does not reject an actually empty queue or get rewritten");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='m'){current->renderables[4]=Ptr(&current->models[2]);Vector(current->object,0x18,current->renderables,5);}};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"new model appended during resource preparation rejects complete group");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='m')current->renderables[1]=Ptr(&current->models[2]);};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"replacement of null gap rejects although old model occurrence counts are unchanged");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='m'){source::BeginDirectScope();source::inputs.clear();}};
    f.Submit(scope);Check(!drawCalls&&source::submissionFailed&&!source::DirectSupportOmitted(f.Support()),"reentrant scope clear cannot invalidate borrowed map storage or continue to draw");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    f.originalSelection[0]=0x123400;Vector(f.selection,0x28,f.originalSelection,1);
    onApi=[](char operation,unsigned){if(operation=='m')current->originalSelection[0]=current->Support();};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"original-selection contents changed with equal header cannot be omitted from new selection");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='m')native_update_source::Invalidate();};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"new native update epoch during resource preparation invalidates old phase witness");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='a')ChangedContent();};
    f.Submit(scope);Check(!drawCalls&&!meshCalls&&!source::DirectSupportOmitted(f.Support()),"texture mutation in CreateMaterial callback aborts before CreateMesh");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onTarget=[](){if(targetCalls==2)ChangedContent();};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"pure final fence catches texture mutation by last camera COM read");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onTarget=[](){if(targetCalls==2){transport::BeginReset(current->Device());transport::EndReset(current->Device());}};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"transport reset during final camera read invalidates prepared mesh and texture");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onTarget=[](){if(targetCalls==2)source::DirectSelectionRestarted(source::directScope);};
    f.Submit(scope);Check(!drawCalls&&source::submissionFailed&&!source::DirectSupportOmitted(f.Support()),"selection restart at last external read never permits a following API draw");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned ordinal){if(operation=='d'&&ordinal==1)ChangedContent();};
    f.Submit(scope);Check(drawCalls==1&&source::submissionFailed&&source::DirectSupportOmitted(f.Support()),"mutation after accepted instance stops all remaining occurrences and quarantines current support");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned ordinal){if(operation=='d'&&ordinal==1)RetireSurfaceResources(true);};
    f.Submit(scope);Check(drawCalls==1&&source::submissionFailed,"prepared API mesh retired by callback cannot be used for next instance");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned ordinal){if(operation=='d'&&ordinal==1){source::DirectSelectionRestarted(source::directScope);source::inputs.clear();}};
    f.Submit(scope);Check(drawCalls==1&&source::submissionFailed&&!source::DirectSupportOmitted(f.Support()),"accepted draw followed by reentry releases old selection ownership and never dereferences old input map");}
}
static void ForwardingAndAllocation() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);f.Submit(scope);
    native_mesh_source::Scope meshScope{};meshScope.valid=true;meshScope.mesh=Ptr(&f.mesh);meshScope.renderer=Ptr(f.renderer);
    meshScope.owner=source::inputs.begin()->second.owner;native_mesh_source::active=&meshScope;
    const auto before=source::directUnexpectedDraws;const auto range=source::inputs.begin()->second.resources.range;
    Check(!source::SkipDirectDraw(f.Device(),range)&&source::submissionFailed&&source::directUnexpectedDraws==before+1,
      "even matching unexpected original DIP is forwarded and disables direct source");
    Check(!source::SkipDirectDraw(nullptr,{D3DPT_TRIANGLESTRIP,90,0,3,10,1}),"foreign device/range cannot be suppressed");
    native_mesh_source::active=nullptr;scope.attempted=true;scene_geometry::BeforeSelect();
    Check(!source::DirectSupportOmitted(f.Support())&&source::directUses.empty(),"before repeated original selection clears prior omission ledger");
    f.originalSelection[0]=f.Support();Vector(f.selection,0x28,f.originalSelection,1);
    scene_geometry::Registry registry{};registry.valid=true;registry.mutationSerial=scene_geometry::MutationSerial();registry.supports={f.Support()};scope.ExtendSelection(Ptr(f.selection),registry);
    Check(scope.marks.empty()&&scope.expanded==std::vector<uint32_t>({f.Support()}),"new original selection support stays in original vector without adapter stamp");}
  for(int after=0;after<3;++after) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    // Arm only after the complete resource preparation at the final camera read.
    static int allocationOffset;allocationOffset=after;
    onTarget=[](){if(targetCalls==2)failAllocationAfter=allocationOffset;};
    bool threw=false;try{f.Submit(scope);}catch(const std::bad_alloc&){threw=true;}
    failAllocationAfter=-1;Check(threw&&!drawCalls&&!source::DirectSupportOmitted(f.Support())&&!source::directRunning,
      "ledger allocation failure before irreversible draw leaves support in fallback and restores running scope");
  }
}
static void AlphaPolicy() {
  for(unsigned policy=0;policy<4;++policy) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));
    f.material.base.renderStates[10]=6; // Native material mapper emits GREATEREQUAL=7.
    f.material.base.renderStates[9]=policy==3?3:0;
    f.states[D3DRS_ALPHATESTENABLE]=1;f.states[D3DRS_ALPHAFUNC]=D3DCMP_GREATEREQUAL;
    opaqueAlphaTest=policy!=2;keepTrivialAlphaTestForComparison=policy==1;
    f.Inputs(scope);const auto nativeBefore=f.material;f.Submit(scope);
    Check(drawCalls==3,"opaque alpha policy retains complete direct group");
    const unsigned expected=policy==0?7:6;
    for(const auto& blend:blends)Check(blend.alphaTestEnabled&&blend.alphaTestCompareOp==expected&&
       blend.alphaTestReferenceValue==(policy==3?3:0),"recording API uses normalized alpha only for enabled trivial opaque policy");
    for(const auto& input:source::inputs)Check(input.second.instance.function==D3DCMP_GREATEREQUAL,
      "normalization changes own API recipe only, preserving original packet alpha input");
    Check(!memcmp(&nativeBefore,&f.material,sizeof(nativeBefore))&&f.states[D3DRS_ALPHAFUNC]==D3DCMP_GREATEREQUAL,
      "direct alpha normalization never writes native material or device state");
  }
}
static void EarlyCamera() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);f.Submit(scope);
    Check(cameraCalls==1&&drawCalls==3&&apiOrder==std::vector<char>({'c','d','d','d'}),"first independent group submits camera exactly once before every instance");
    Check(!memcmp(cameras[0].view,f.camera.viewMatrix,64)&&!memcmp(cameras[0].projection,f.camera.projectionMatrix,64),"early camera preserves exact signed-zero and perspective matrix bits");
    const auto before=comCalls;native_camera_source::BeforeFirstSceneInstance(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(cameraCalls==1&&comCalls==before,"repeated early helper is a no-op after first camera attempt");
    native_camera_source::AtDraw(f.Device(),Ptr(&f.scene),Ptr(&f.camera));Check(cameraCalls==1,"later first D3D draw cannot issue a second SetupCamera");}
  for(unsigned reason=0;reason<4;++reason) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);
    if(reason==0)drawId=1;if(reason==1)native_camera_source::attemptedFrame=frameId;
    if(reason==2)native_camera_source::observedFrame=frameId;if(reason==3)native_camera_source::submitEnabled=false;
    const auto before=comCalls;native_camera_source::BeforeFirstSceneInstance(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(!cameraCalls&&comCalls==before,"early helper does nothing after prior draw, attempted/observed camera or disabled API source");
    f.Submit(scope);Check(!drawCalls&&!cameraCalls,"missing accepted current camera never authorizes independent instance");
  }
  for(unsigned slot=81;slot<=84;++slot) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);f.deviceTable[slot]=nullptr;
    const auto before=comCalls;native_camera_source::BeforeFirstSceneInstance(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(!cameraCalls&&comCalls==before,"each tracked draw slot must be present before drawId zero proves early window");
    f.Submit(scope);Check(!drawCalls&&!cameraCalls,"missing draw-hook coverage cannot bootstrap direct camera");
  }
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);++native_camera_source::ownerThread;
    const auto before=comCalls;native_camera_source::BeforeFirstSceneInstance(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(!cameraCalls&&comCalls==before,"foreign camera owner thread cannot perform early SetupCamera");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);f.deviceView._41+=1;
    f.Submit(scope);Check(!cameraCalls&&!drawCalls&&native_camera_source::attemptedFrame==frameId,"native versus D3D camera mismatch closes early window without Setup or instance");
    f.deviceView._41-=1;native_camera_source::BeforeFirstSceneInstance(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(!cameraCalls,"later matching transforms cannot retry closed mismatch window");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);failCamera=true;
    f.Submit(scope);Check(cameraCalls==1&&!drawCalls&&native_camera_source::submittedFrame!=frameId,"normal SetupCamera failure never authorizes instance");
    failCamera=false;f.Submit(scope);Check(cameraCalls==1&&!drawCalls,"SetupCamera error does not cause a late retry");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);f.api.SetupCamera=nullptr;
    f.Submit(scope);Check(!cameraCalls&&!drawCalls&&native_camera_source::attemptedFrame==frameId,"missing SetupCamera capability closes window with fallback");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);
    onTarget=[](){onTarget=nullptr;++drawId;};
    f.Submit(scope);Check(!cameraCalls&&!drawCalls,"draw reentry during final target read closes first-instance window before SetupCamera");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);
    onTarget=[](){onTarget=nullptr;native_camera_source::AtDraw(current->Device(),Ptr(&current->scene),Ptr(&current->camera));};
    f.Submit(scope);Check(cameraCalls==1&&native_camera_source::submittedFrame==frameId,"nested AtDraw during final target read accepts one camera and prevents outer duplicate SetupCamera");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='c')++frameId;};
    f.Submit(scope);Check(cameraCalls==1&&!drawCalls&&native_camera_source::submittedFrame!=frameId,"frame transition inside SetupCamera cannot be credited to prepared direct packet or current submitted frame");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.EarlyCamera();f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='c'){scene_geometry::Scope nested(Ptr(&current->scene),Ptr(&current->camera));}};
    f.Submit(scope);Check(cameraCalls==1&&!drawCalls&&source::submissionFailed&&scene_geometry::active==&scope,
      "nested scene scope during SetupCamera invalidates operation after API and restores outer TLS");}
}
static void SceneFrameOrdering() {
  {Fixture f;f.SceneLights();f.EarlyCamera();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);f.Submit(scope);
    const auto light=std::find(apiOrder.begin(),apiOrder.end(),'l'),instance=std::find(apiOrder.begin(),apiOrder.end(),'d');
    Check(cameraCalls==1&&lightCreates==1&&lightDraws==1&&drawCalls==3&&apiOrder.front()=='c'&&light<instance,
      "API-only frame prepares camera then complete scene light before every instance");
    Check(drawId==0&&lightPositions.size()==1&&lightPositions[0].x==-100&&lightPositions[0].z==-100,
      "point light behind camera is transmitted without any original D3D draw");
    const auto before=lightDraws;PrepareSceneLightsForScene(f.Device(),Ptr(&f.scene),Ptr(&f.camera));
    Check(lightDraws==before&&f.target.refs==1,"later matching boundary does not replay lights and balances target references");}
  for(unsigned failure=0;failure<3;++failure) {
    Fixture f;f.SceneLights();f.EarlyCamera();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    if(failure==0)failLightCreate=true;if(failure==1)failLightDraw=true;if(failure==2)f.api.DrawLightInstance=nullptr;
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"incomplete scene light batch retains original geometry fallback");
    Check((failure==0&&lightCreates==1&&!lightDraws)||(failure==1&&lightCreates==1&&lightDraws==1)||
      (failure==2&&!lightCreates&&!lightDraws),"failure fixture reaches exactly the intended light API boundary");
  }
  for(unsigned mutation=0;mutation<5;++mutation) {
    Fixture f;f.SceneLights();f.EarlyCamera();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned change,hits;change=mutation;hits=0;
    onApi=[](char operation,unsigned){if(operation!='l')return;
      ++hits;
      if(change==0)++frameId;if(change==1)native_camera_source::Reset();
      if(change==2)current->ChangeSceneLight();if(change==3)current->camera.viewMatrix[12]+=1;
      if(change==4)source::DirectSelectionRestarted(source::directScope);};
    f.Submit(scope);Check(!drawCalls&&!source::DirectSupportOmitted(f.Support()),"light callback mutation cannot authorize a stale independent frame");
    Check(hits==1&&lightDraws==1,"mutation ran inside the intended light submission callback");
  }
  for(unsigned boundary=0;boundary<2;++boundary)for(unsigned mutation=0;mutation<6;++mutation) {
    Fixture f;f.SceneLights();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    static unsigned change,hits;change=mutation;hits=0;
    const auto mutate=[] {
      if(source::directFrameScheduler.Stage()!=winx_remix::FrameScheduler::Phase::Lights||hits)return;
      ++hits;
      if(change==0)++frameId;if(change==1)native_camera_source::Reset();
      if(change==2)current->camera.viewMatrix[12]+=1;if(change==3)source::DirectSelectionRestarted(source::directScope);
      if(change==4)ResetSceneLights();if(change==5)sceneLightsEnabled=false;
    };
    if(boundary==0)onTarget=mutate;else onTargetRelease=mutate;
    f.Submit(scope);
    Check(hits==1&&!lightCreates&&!lightDraws&&!lightPolicyCalls&&!drawCalls&&
      !source::DirectSupportOmitted(f.Support())&&f.target.refs==1,
      "COM target/read release mutation before SyncLights sends no stale light batch or geometry");
  }
  {Fixture f;f.SceneLights();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned){if(operation=='m')current->ChangeSceneLight();};
    f.Submit(scope);Check(lightDraws==1&&meshCalls==1&&!drawCalls&&!source::DirectSupportOmitted(f.Support()),"payload change during mesh preparation invalidates lighting ticket before instance");}
  {Fixture f;f.SceneLights();scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);
    onApi=[](char operation,unsigned ordinal){if(operation=='d'&&ordinal==1)current->ChangeSceneLight();};
    f.Submit(scope);
    Check(drawCalls==1&&source::submissionFailed&&source::DirectSupportOmitted(f.Support()),
      "light mutation after irreversible instance stops remaining occurrences without duplicate fallback");}
  {Fixture f;f.SceneLights();Put(f.sceneLightManager,0x10,0);Put(f.sceneLightManager,0x14,0);Put(f.sceneLightManager,0x18,0);
    scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Inputs(scope);f.Submit(scope);
    Check(!lightCreates&&!lightDraws&&drawCalls==3,"empty direct registry is a complete batch without a fabricated fallback lamp");}
}
static void Run(){Basic();Rejections();PartialFailure();PacketSourceTransitions();TextureReadReentry();TextureLifetimeTransitions();TextureBindingTransitions();SequenceAndReentry();ForwardingAndAllocation();AlphaPolicy();EarlyCamera();SceneFrameOrdering();Check(liveMeshes.empty()&&liveMaterials.empty()&&liveLights.empty(),"all recording API ownership retired at fixture end");}
}
#if !defined(WINX_INDEPENDENT_SUBMIT_FIXTURE_ONLY)
int main(){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);try{direct_test::Watchdog watchdog;direct_test::Run();printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"ownedComCalls\":%u}\n",direct_test::checks,direct_test::comCalls);return 0;}
catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
#endif
