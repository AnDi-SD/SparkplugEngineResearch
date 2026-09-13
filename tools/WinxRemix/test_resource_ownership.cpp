// Own CPU ownership fixture: recording API, real cache allocation failures and
// public-operation reentry. No game, COM method, graphics device or GPU executes.
#include <cstddef>
#include <cstdlib>
#include <new>
static thread_local int ownershipAllocationsUntilFailure=-1;
void* operator new(std::size_t size) {
  if(ownershipAllocationsUntilFailure==0){ownershipAllocationsUntilFailure=-1;throw std::bad_alloc();}
  if(ownershipAllocationsUntilFailure>0)--ownershipAllocationsUntilFailure;
  if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"

namespace ownership_test {
static unsigned checks,materialCalls,meshCalls,materialDeletes,meshDeletes,liveMaterials,liveMeshes;
static void Check(bool ok,const char* message){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Watchdog {
  HANDLE event=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* e){if(WaitForSingleObject(e,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0521a30u);return 0;}
  Watchdog(){event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
  ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
enum class Action {None,Reenter,Retire,AdvanceFrame};
struct Behavior {bool fail=false,nullHandle=false,throwAfter=false,armAllocation=false;Action action=Action::None;};
static Behavior materialBehavior,meshBehavior;
static bool rejectMeshDelete,rejectMaterialDelete,throwMeshDelete,throwMaterialDelete;
static bool retireOnDelete;
struct Record {bool live=false,mesh=false;remixapi_MaterialHandle material=nullptr;};
static Record records[4096]{};
static unsigned nextId=1;
static std::vector<remixapi_HardcodedVertex> vertices;
static remixapi_MaterialHandle reentryMaterial;
static uint64_t reentryKey;
static remixapi_MaterialHandle Material(uint64_t key) {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.hash=key;
  return CreateSurfaceMaterialOwned(info,operation);
}
static unsigned Id(const void* handle){return static_cast<unsigned>(reinterpret_cast<uintptr_t>(handle));}
static void Act(Action action) {
  if(action==Action::Reenter) {
    const auto before=materialCalls+meshCalls;
    Check(!Material(reentryKey)&&!Material(reentryKey+10000),"same-key and different-key reentrant materials reject");
    Check(!SurfaceGeometryResource(vertices,reentryMaterial),"reentrant geometry rejects");
    Check(!EnsureSurfaceResourceRoom(0,0,0),"reentrant pressure rejects");
    Check(before==materialCalls+meshCalls,"reentry never invokes a second API Create");
    Check(!SurfaceMeshCurrent(reinterpret_cast<remixapi_MeshHandle>(1)),"current-resource fence rejects during an operation");
  } else if(action==Action::Retire)RetireSurfaceResources(true);
  else if(action==Action::AdvanceFrame)++frameId;
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMaterial(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* output) {
  ++materialCalls;Check(info&&output,"material arguments");
  Check(surfaceMaterials.count(info->hash)&&!surfaceMaterials.at(info->hash).handle&&!surfaceMaterials.at(info->hash).usable,
    "material cache node owns pending output before API Create");
  const auto behavior=materialBehavior;
  if(!behavior.nullHandle) {
    Check(nextId<4096,"bounded recording handles");const auto id=nextId++;
    records[id]={true,false,nullptr};*output=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(id));++liveMaterials;
  }
  Act(behavior.action);
  if(behavior.throwAfter)throw std::bad_alloc();
  if(behavior.armAllocation)ownershipAllocationsUntilFailure=0;
  return behavior.fail?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* output) {
  ++meshCalls;Check(info&&output&&info->surfaces_count==1,"mesh arguments");
  const auto& surface=info->surfaces_values[0];
  Check(surface.vertices_count==3&&surface.indices_count==3&&surface.indices_values[0]==0&&surface.indices_values[2]==2,
    "real sequential index payload reaches API");
  const auto material=Id(surface.material);Check(material<4096&&records[material].live&&!records[material].mesh,"mesh uses a live material");
  Check(surfaceMeshes.count(info->hash)&&!surfaceMeshes.at(info->hash).handle&&!surfaceMeshes.at(info->hash).usable,
    "mesh cache node and byte reservation exist before API Create");
  const auto behavior=meshBehavior;
  if(!behavior.nullHandle) {
    Check(nextId<4096,"bounded recording mesh handles");const auto id=nextId++;
    records[id]={true,true,surface.material};*output=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(id));++liveMeshes;
  }
  Act(behavior.action);
  if(behavior.throwAfter)throw std::bad_alloc();
  if(behavior.armAllocation)ownershipAllocationsUntilFailure=0;
  return behavior.fail?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMesh(remixapi_MeshHandle handle) {
  ++meshDeletes;const auto id=Id(handle);Check(id<4096&&records[id].live&&records[id].mesh,"mesh deletion has one live owner");
  if(retireOnDelete)RetireSurfaceResources(true);
  if(throwMeshDelete)throw std::bad_alloc();
  if(rejectMeshDelete)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  records[id].live=false;--liveMeshes;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMaterial(remixapi_MaterialHandle handle) {
  ++materialDeletes;const auto id=Id(handle);Check(id<4096&&records[id].live&&!records[id].mesh,"material deletion has one live owner");
  bool referenced=false;for(const auto& r:records)referenced|=r.live&&r.mesh&&r.material==handle;
  Check(!referenced,"material outlives all recording meshes");
  if(retireOnDelete)RetireSurfaceResources(true);
  if(throwMaterialDelete)throw std::bad_alloc();
  if(rejectMaterialDelete)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  records[id].live=false;--liveMaterials;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static void Clean() {
  ownershipAllocationsUntilFailure=-1;materialBehavior={};meshBehavior={};
  rejectMeshDelete=rejectMaterialDelete=throwMeshDelete=throwMaterialDelete=retireOnDelete=false;
  RetireSurfaceResources(true);
  Check(!liveMeshes&&!liveMaterials&&surfaceMeshes.empty()&&surfaceMaterials.empty()&&!surfaceMeshBytes,
    "teardown releases every API handle, cache node and byte reservation");
  Check(!surfaceResourceBusy&&!surfaceRetirePending&&!surfaceRetireDraining,"teardown leaves no operation or deferred retirement");
}
static void AllocationAndCache() {
  Clean();frameId=100;auto before=materialCalls;
  ownershipAllocationsUntilFailure=0;auto material=Material(101);ownershipAllocationsUntilFailure=-1;
  Check(!material&&materialCalls==before&&surfaceMaterials.empty()&&!liveMaterials,"material bad_alloc precedes Create and leaks nothing");
  materialBehavior.armAllocation=true;material=Material(101);
  const auto armed=ownershipAllocationsUntilFailure;ownershipAllocationsUntilFailure=-1;
  Check(material&&armed==0,"material success needs no allocation after API output");materialBehavior={};
  Check(Material(101)==material&&materialCalls==before+1,"repeat material key returns the owned handle once");
  const auto epoch=SurfaceResourceEpoch();const auto calls=meshCalls;
  for(int allocation=0;allocation<2;++allocation) {
    ownershipAllocationsUntilFailure=allocation;auto mesh=SurfaceGeometryResource(vertices,material);ownershipAllocationsUntilFailure=-1;
    Check(!mesh&&meshCalls==calls&&surfaceMeshes.empty()&&!surfaceMeshBytes&&liveMaterials==1,
      "index-vector and mesh-cache bad_alloc both occur before API Create");
  }
  meshBehavior.armAllocation=true;auto mesh=SurfaceGeometryResource(vertices,material);
  const auto meshArmed=ownershipAllocationsUntilFailure;ownershipAllocationsUntilFailure=-1;meshBehavior={};
  Check(mesh&&meshArmed==0&&SurfaceResourceEpoch()>epoch,"mesh success needs no allocation after API output");
  Check(SurfaceGeometryResource(vertices,material)==mesh&&meshCalls==calls+1,"repeat geometry key returns one live mesh");
  Check(SurfaceMeshCurrent(mesh,material)&&!SurfaceMeshCurrent(mesh,reinterpret_cast<remixapi_MaterialHandle>(999))&&
    !SurfaceMeshCurrent(nullptr),"noAPI current-handle fence validates both dependencies");
  const auto deletes=meshDeletes+materialDeletes;
  Check(!EnsureSurfaceResourceRoom(1,surfaceMeshByteLimit,0,material)&&deletes==meshDeletes+materialDeletes,
    "pressure cannot retire a current-frame prepared group");
  const auto beforeRetire=SurfaceResourceEpoch();Clean();
  Check(SurfaceResourceEpoch()>beforeRetire&&!SurfaceMeshCurrent(mesh,material),"retirement invalidates both epoch and handle fence");
}
static void Failures() {
  for(unsigned kind=0;kind<2;++kind)for(unsigned mode=0;mode<4;++mode) {
    Clean();frameId=200;
    auto& behavior=kind?meshBehavior:materialBehavior;
    remixapi_MaterialHandle material=kind?Material(200):nullptr;
    behavior.nullHandle=mode<2;behavior.fail=mode==1||mode==2;behavior.throwAfter=mode==3;
    auto before=kind?meshDeletes:materialDeletes;
    auto handle=kind?reinterpret_cast<void*>(SurfaceGeometryResource(vertices,material)):reinterpret_cast<void*>(Material(201));
    Check(!handle,"null, failing and throwing Create never returns a usable resource");
    Check((kind?meshDeletes:materialDeletes)==before+(mode>=2?1u:0u),"nonnull output is destroyed even after API failure or exception");
    Check(surfaceMeshes.empty()&&!surfaceMeshBytes&&(kind?liveMaterials==1:liveMaterials==0),"failed Create releases its own reservation");
  }
  Clean();frameId=210;materialBehavior.fail=true;rejectMaterialDelete=true;
  Check(!Material(211)&&liveMaterials==1&&surfaceMaterials.size()==1&&!surfaceMaterials.begin()->second.usable,
    "failed material destruction retains an unusable owned quarantine");
  const auto calls=materialCalls;Check(!Material(211)&&materialCalls==calls,"quarantined repeated key cannot create a duplicate or be reused");
  Clean();frameId=220;auto material=Material(220);meshBehavior.throwAfter=true;throwMeshDelete=true;
  Check(!SurfaceGeometryResource(vertices,material)&&liveMeshes==1&&surfaceMeshes.size()==1&&!surfaceMeshes.begin()->second.usable&&surfaceMeshBytes,
    "throwing mesh Create plus throwing delete retains bytes and ownership");
  auto mesh=surfaceMeshes.begin()->second.handle;
  Check(!SurfaceMeshCurrent(mesh,material),"quarantined mesh cannot pass final submission fence");
  RetireSurfaceResources(true);
  Check(liveMaterials==1&&liveMeshes==1,"failed quarantined mesh still pins its material during retirement");
  Clean();
  materialBehavior.throwAfter=true;throwMaterialDelete=true;Check(!Material(221)&&liveMaterials==1,"throwing material deletion remains retryable");Clean();
}
static void Reentry() {
  Clean();frameId=300;reentryKey=300;materialBehavior.action=Action::Reenter;
  auto material=Material(reentryKey);Check(material&&liveMaterials==1,"material reentry preserves original-once success");materialBehavior={};
  reentryMaterial=material;meshBehavior.action=Action::Reenter;
  auto mesh=SurfaceGeometryResource(vertices,material);Check(mesh&&liveMeshes==1,"mesh reentry preserves original-once success");Clean();
  // Reproduces a retirement during sampler/asset work before a pending node exists.
  const auto calls=materialCalls;const auto epoch=SurfaceResourceEpoch();
  {SurfaceResourceOperation operation;Check(operation.Stable(),"empty-cache operation starts valid");
    RetireSurfaceResources(true);Check(!operation.Stable(),"empty-cache retirement invalidates in-progress material work");
    remixapi_MaterialInfo info{};info.hash=301;Check(!CreateSurfaceMaterialOwned(info,operation),"invalidated empty-cache operation cannot create");}
  Check(materialCalls==calls&&SurfaceResourceEpoch()>epoch&&!surfaceRetirePending,"empty-cache retirement is observed and drained");
  materialBehavior.action=Action::Retire;Check(!Material(302)&&!liveMaterials,"retirement during material Create cancels and releases its output");Clean();
  material=Material(303);meshBehavior.action=Action::Retire;
  Check(!SurfaceGeometryResource(vertices,material)&&!liveMeshes&&!liveMaterials,"retirement during mesh Create drains mesh before material");Clean();
  materialBehavior.action=Action::AdvanceFrame;Check(!Material(304)&&!liveMaterials,"frame change during material Create rejects stale output");Clean();
  material=Material(305);meshBehavior.action=Action::AdvanceFrame;
  Check(!SurfaceGeometryResource(vertices,material)&&!liveMeshes&&liveMaterials==1,"frame change during mesh Create cancels only its output");Clean();
  material=Material(306);mesh=SurfaceGeometryResource(vertices,material);retireOnDelete=true;
  RetireSurfaceResources(true);Check(!liveMeshes&&!liveMaterials,"recursive retirement on both delete APIs drains without duplicate destruction");Clean();
  material=Material(307);retireOnDelete=rejectMaterialDelete=true;
  const auto attempts=materialDeletes;RetireSurfaceResources(true);
  Check(materialDeletes-attempts<=2&&liveMaterials==1&&surfaceRetirePending,"repeated retirement with delete failure stays bounded and pending");
  const auto createCalls=materialCalls;const auto retryAttempts=materialDeletes;
  Check(!Material(307)&&materialCalls==createCalls&&materialDeletes-retryAttempts<=1,
    "cached handle cannot escape an undrained retirement before API recovery");
  retireOnDelete=rejectMaterialDelete=false;auto replacement=Material(307);
  Check(replacement&&replacement!=material&&liveMaterials==1&&!surfaceRetirePending,"recovery drains old owner before allocating replacement");Clean();
}
static void EntryPoints(remixapi_Interface& api) {
  Clean();frameId=400;const auto createCalls=materialCalls;
  api.DestroyMaterial=nullptr;Check(!Material(400)&&materialCalls==createCalls,"missing material destroy capability rejects before Create");api.DestroyMaterial=DeleteMaterial;
  auto material=Material(401);const auto geometryCalls=meshCalls;api.DestroyMesh=nullptr;
  Check(!SurfaceGeometryResource(vertices,material)&&meshCalls==geometryCalls,"missing mesh destroy capability rejects before Create");api.DestroyMesh=DeleteMesh;Clean();
  Check(GetCurrentDirectoryW(MAX_PATH,surfaceAssetDirectory)>0,"owned fixture asset directory");
  native_material_source::Sampler sampler{};sampler.u=sampler.v=D3DTADDRESS_WRAP;sampler.mag=D3DTEXF_POINT;
  material_channels::Plan plan{};plan.albedo={{1,1,1}};DWORD srgb=0;
  const uint64_t texture=0x12345,values[]={texture,sampler.u,sampler.v,sampler.mag};
  std::string descriptor="winx-independent-ffp-v1";descriptor.append(reinterpret_cast<const char*>(values),sizeof(values));
  descriptor.append(reinterpret_cast<const char*>(&plan.albedo),sizeof(plan.albedo));descriptor.append(reinterpret_cast<const char*>(&plan.emission),sizeof(plan.emission));
  const auto hash=XXH3_64bits(descriptor.data(),descriptor.size());
  wchar_t channelPath[MAX_PATH]{},legacyPath[MAX_PATH]{};
  swprintf_s(channelPath,L"%s\\%016llX-albedo.dds",surfaceAssetDirectory,static_cast<unsigned long long>(hash));
  swprintf_s(legacyPath,L"%s\\%016llX.dds",surfaceAssetDirectory,static_cast<unsigned long long>(texture));
  // Explicit file-existence fixture; not DDS decoding/export evidence. Production
  // cache-hit file paths skip every COM call, so a null device proves that boundary.
  for(auto path:{channelPath,legacyPath}){FILE* file=nullptr;Check(_wfopen_s(&file,path,L"wb")==0&&file,"create owned asset-existence sentinel");fputs("CPU ownership fixture: no DDS decoding",file);fclose(file);}
  material=SurfaceChannelMaterial(nullptr,texture,plan,&sampler,nullptr,&srgb);
  Check(material&&SurfaceChannelMaterial(nullptr,texture,plan,&sampler,nullptr,&srgb)==material,"channel entry point reaches common owned Create and cache hit without COM");
  auto legacy=SurfaceMaterial(nullptr,texture,&sampler);
  Check(legacy&&legacy!=material&&SurfaceMaterial(nullptr,texture,&sampler)==legacy,"legacy entry point reaches common owned Create and cache hit without COM");Clean();
}
}
int main() {
  using namespace ownership_test;Watchdog watchdog;vertices.resize(3);vertices[1].position[0]=1;vertices[2].position[1]=1;
  remixapi_Interface recording{};recording.CreateMaterial=CreateMaterial;recording.CreateMesh=CreateMesh;
  recording.DestroyMaterial=DeleteMaterial;recording.DestroyMesh=DeleteMesh;testRemixApi=&recording;
  AllocationAndCache();Failures();Reentry();EntryPoints(recording);Clean();
  printf("{\"status\":\"PASS\",\"checks\":%u,\"materialCreates\":%u,\"meshCreates\":%u,\"materialDeleteAttempts\":%u,\"meshDeleteAttempts\":%u,\"reentryRejected\":%u,\"remaining\":0,\"gpu\":false,\"nativeCode\":false}\n",
    checks,materialCalls,meshCalls,materialDeletes,meshDeletes,surfaceResourceReentryRejected);
}
