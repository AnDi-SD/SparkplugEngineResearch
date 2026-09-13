// Own adapter lifetime test. A recording API refuses use-after-free and duplicate
// deletion; no game code, graphics device or external process is executed.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <cstdlib>
static unsigned checks;
static bool rejectMesh,rejectMaterial;
static std::map<remixapi_MeshHandle,remixapi_MaterialHandle> modelMeshes;
static std::set<remixapi_MaterialHandle> modelMaterials;
static std::vector<char> deletions;
static std::vector<remixapi_MeshHandle> meshAttempts;
static std::vector<remixapi_MaterialHandle> materialAttempts;
static std::set<remixapi_MeshHandle> refusedMeshes;
static void Check(bool ok,const char* message) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMesh(remixapi_MeshHandle mesh) {
  meshAttempts.push_back(mesh);
  Check(modelMeshes.count(mesh)==1,"delete an existing mesh exactly once");
  if(rejectMesh||refusedMeshes.count(mesh))return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  modelMeshes.erase(mesh);deletions.push_back('m');return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMaterial(remixapi_MaterialHandle material) {
  materialAttempts.push_back(material);
  Check(modelMaterials.count(material)==1,"delete an existing material exactly once");
  for(const auto& mesh:modelMeshes)Check(mesh.second!=material,"no live mesh references the material");
  if(rejectMaterial)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  modelMaterials.erase(material);deletions.push_back('a');return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_MaterialHandle Material(uint64_t key,unsigned seen) {
  auto handle=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(key));
  Check(modelMaterials.insert(handle).second,"new unique material");
  surfaceMaterials.emplace(key,SurfaceMaterialEntry{handle,seen});return handle;
}
static void Mesh(uint64_t key,remixapi_MaterialHandle material,unsigned seen,size_t bytes=128) {
  auto handle=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(key));
  Check(modelMaterials.count(material)==1,"mesh creation needs a live material");
  modelMeshes.emplace(handle,material);surfaceMeshes.emplace(key,SurfaceMesh{handle,seen,bytes,material});surfaceMeshBytes+=bytes;
}
static void Clean() {
  rejectMesh=rejectMaterial=false;refusedMeshes.clear();RetireSurfaceResources(true);
  Check(surfaceMeshes.empty()&&surfaceMaterials.empty()&&modelMeshes.empty()&&modelMaterials.empty()&&!surfaceMeshBytes,
    "fixture teardown releases all actual recording API ownership");
  deletions.clear();meshAttempts.clear();materialAttempts.clear();
}
static void Pressure(remixapi_Interface& recording) {
  // Real production limits, synthetic API resources. The bytes are accounting
  // inputs; this CPU test neither allocates vertex payloads nor starts D3D/GPU.
  Clean();frameId=2000;
  auto shared=Material(1,0);Mesh(1,shared,0);Material(2,0);
  Check(EnsureSurfaceResourceRoom(1,128,1)&&deletions.empty(),"sufficient room preserves young and old cached resources");
  Check(!EnsureSurfaceResourceRoom(1,size_t(-1),0)&&deletions.empty(),"overflowing incoming byte request refuses without destroying resources");
  Check(!EnsureSurfaceResourceRoom(513,0,0)&&!EnsureSurfaceResourceRoom(0,0,257)&&deletions.empty(),
    "impossible count requests do not evict useful resources");Clean();

  frameId=2100;shared=Material(1,0);const auto pinned=Material(2,0);Material(3,0);
  for(unsigned i=1;i<=512;++i)Mesh(i,shared,i==177?1700:2000);
  surfaceMeshes.at(17).frame=frameId;
  const auto meshesBefore=surfaceMeshDestroys,materialsBefore=surfaceMaterialDestroys;
  Check(EnsureSurfaceResourceRoom(1,128,0,pinned),"full 512-mesh cache makes room before TTL expires");
  Check(deletions==std::vector<char>({'a','m'})&&!surfaceMaterials.count(3)&&!surfaceMeshes.count(177)&&surfaceMeshes.count(17),
    "pressure clears unused material first then the oldest eligible mesh, preserving current-frame mesh");
  Check(surfaceMaterials.at(2).handle==pinned&&modelMaterials.count(pinned)&&surfaceMaterials.count(1),
    "incoming mesh material is explicitly pinned and shared material retains its live users");
  Check(surfaceMeshDestroys==meshesBefore+1&&surfaceMaterialDestroys==materialsBefore+1&&surfaceMeshBytes==511*128,
    "pressure updates successful destruction and byte accounting exactly");
  Mesh(600,pinned,frameId);surfaceMaterials.at(2).frame=frameId;
  Check(surfaceMeshes.size()==512&&modelMeshes.count(reinterpret_cast<remixapi_MeshHandle>(600)),
    "new mesh can consume reclaimed slot and still-live pinned material");Clean();

  frameId=2200;
  for(unsigned i=1;i<=256;++i)Material(i,i==127?2100:frameId);
  Check(EnsureSurfaceResourceRoom(0,0,1)&&surfaceMaterials.size()==255&&!surfaceMaterials.count(127)&&meshAttempts.empty(),
    "full material cache first reclaims an orphan without deleting meshes");
  Material(300,frameId);Check(surfaceMaterials.size()==256,"new material fits unchanged 256-resource limit");Clean();

  frameId=2300;
  for(unsigned i=1;i<=256;++i){auto material=Material(i,i==1?frameId:2200);Mesh(i,material,2200);}
  surfaceMeshes.at(1).frame=0;surfaceMeshes.at(2).frame=1;surfaceMeshes.at(3).frame=frameId;
  surfaceMeshes.at(4).frame=2;Mesh(500,surfaceMaterials.at(4).handle,3);
  const auto keep=surfaceMaterials.at(2).handle;
  Check(EnsureSurfaceResourceRoom(0,0,1,keep),"material pressure releases an older mesh dependency group");
  Check(deletions==std::vector<char>({'m','m','a'})&&!surfaceMeshes.count(4)&&!surfaceMeshes.count(500)&&!surfaceMaterials.count(4),
    "shared orphan material is released only after its last old mesh");
  Check(surfaceMeshes.count(1)&&surfaceMeshes.count(2)&&surfaceMeshes.count(3)&&surfaceMaterials.count(1)&&surfaceMaterials.count(2),
    "material-only pressure skips current-frame material, explicit pin and material with a current-frame mesh");Clean();

  constexpr size_t mib=1024*1024;frameId=2400;
  auto first=Material(1,2200),second=Material(2,2300),current=Material(3,frameId),incoming=Material(4,frameId);
  Mesh(1,first,2200,16*mib);Mesh(2,second,2300,32*mib);Mesh(3,current,frameId,16*mib);
  Check(EnsureSurfaceResourceRoom(1,40*mib,0,incoming),"64-MiB pressure reclaims enough bytes across multiple old meshes");
  Check(deletions==std::vector<char>({'m','a','m','a'})&&surfaceMeshBytes==16*mib&&surfaceMeshes.count(3)&&surfaceMaterials.count(4),
    "byte pressure destroys each orphan after its mesh and preserves current frame resources");
  Mesh(4,incoming,frameId,40*mib);Check(surfaceMeshBytes==56*mib,"incoming mesh accounting remains within 64 MiB");Clean();

  frameId=1;shared=Material(1,frameId);Mesh(1,shared,0xfffffffbu,24*mib);
  Mesh(2,shared,0xfffffffeu,24*mib);Mesh(3,shared,frameId,16*mib);
  Check(EnsureSurfaceResourceRoom(1,16*mib,0,shared)&&!surfaceMeshes.count(1)&&surfaceMeshes.count(2)&&surfaceMeshes.count(3),
    "unsigned age selects oldest mesh across frame-counter wrap");Clean();

  frameId=2500;shared=Material(1,frameId);
  for(unsigned i=1;i<=512;++i)Mesh(i,shared,frameId);
  const auto blockedBytes=surfaceMeshBytes;
  Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&meshAttempts.empty()&&materialAttempts.empty()&&surfaceMeshBytes==blockedBytes,
    "all-current-frame mesh pressure leaves fallback and ownership intact");
  Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&deletions.empty(),"repeated exhausted-frame requests never evict current handles");Clean();

  frameId=2600;
  for(unsigned i=1;i<=256;++i){auto material=Material(i,frameId);Mesh(i,material,2500);}
  Check(!EnsureSurfaceResourceRoom(0,0,1)&&meshAttempts.empty()&&materialAttempts.empty(),
    "material pressure does not destroy old meshes when every material is protected this frame");Clean();

  frameId=2700;shared=Material(1,frameId);
  for(unsigned i=1;i<=512;++i)Mesh(i,shared,2600);
  rejectMesh=true;const auto failedBefore=surfaceResourceFailures,destroyedBefore=surfaceMeshDestroys;
  Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&meshAttempts.size()==512&&surfaceResourceFailures==failedBefore+512,
    "all failing mesh deletions make one bounded attempt per candidate");
  Check(surfaceMeshes.size()==512&&surfaceMeshBytes==512*128&&surfaceMeshDestroys==destroyedBefore&&modelMeshes.size()==512,
    "failed deletions retain complete ownership, dependencies and byte accounting");
  rejectMesh=false;Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&meshAttempts.size()==512,
    "later draws do not retry failed mesh deletion in the same frame");
  ++frameId;Check(EnsureSurfaceResourceRoom(1,128,0,shared)&&meshAttempts.size()==513&&surfaceMeshDestroys==destroyedBefore+1,
    "next frame retries and reclaims one old mesh after API recovery");Clean();

  frameId=2800;shared=Material(1,frameId);
  for(unsigned i=1;i<=512;++i)Mesh(i,shared,2700);
  refusedMeshes.insert(surfaceMeshes.at(1).handle);
  Check(EnsureSurfaceResourceRoom(1,128,0,shared)&&meshAttempts.size()==2&&surfaceMeshes.count(1)&&!surfaceMeshes.count(2),
    "one oldest-mesh failure preserves its handle and proceeds to another eligible mesh");Clean();

  frameId=2900;for(unsigned i=1;i<=256;++i)Material(i,2800);
  rejectMaterial=true;const auto rejectedBefore=surfaceResourceFailures,materialDestroyedBefore=surfaceMaterialDestroys;
  Check(!EnsureSurfaceResourceRoom(0,0,1)&&materialAttempts.size()==256&&surfaceResourceFailures==rejectedBefore+256&&
    surfaceMaterialDestroys==materialDestroyedBefore&&surfaceMaterials.size()==256,
    "failing orphan deletion preserves material slots and counts only errors");
  rejectMaterial=false;Check(!EnsureSurfaceResourceRoom(0,0,1)&&materialAttempts.size()==256,
    "failed materials receive no same-frame retry across new allocation requests");
  ++frameId;Check(EnsureSurfaceResourceRoom(0,0,1)&&materialAttempts.size()==257&&surfaceMaterialDestroys==materialDestroyedBefore+1,
    "material retry after frame advance restores allocation room");Clean();

  frameId=3000;for(unsigned i=1;i<=256;++i)Material(i,i==1?2900:frameId);
  Mesh(1,surfaceMaterials.at(1).handle,2900);rejectMaterial=true;
  Check(!EnsureSurfaceResourceRoom(0,0,1)&&modelMeshes.empty()&&surfaceMeshes.empty()&&!surfaceMeshBytes&&surfaceMaterials.size()==256,
    "material deletion failure after mesh eviction leaves an owned retryable orphan");
  Check(meshAttempts.size()==1&&materialAttempts.size()==1,"unreclaimable other materials do not cause extra mesh destruction");
  rejectMaterial=false;++frameId;
  Check(EnsureSurfaceResourceRoom(0,0,1)&&!surfaceMaterials.count(1),"later orphan retry finishes the dependency deletion");Clean();

  frameId=3100;shared=Material(1,3000);Mesh(1,shared,3000,64*mib);
  recording.DestroyMaterial=nullptr;
  Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&deletions.empty()&&surfaceMeshBytes==64*mib,
    "missing delete capability refuses pressure without releasing partial ownership");
  recording.DestroyMaterial=DeleteMaterial;testRemixApi=nullptr;
  Check(!EnsureSurfaceResourceRoom(1,128,0,shared)&&deletions.empty(),"unavailable API leaves resources for the original fallback");
  testRemixApi=&recording;Check(EnsureSurfaceResourceRoom(1,128,0,shared)&&!surfaceMeshBytes&&surfaceMaterials.size()==1,
    "restored API can reclaim old mesh while preserving the explicitly pinned material");Clean();
}
int main() {
  remixapi_Interface recording{};recording.DestroyMesh=DeleteMesh;recording.DestroyMaterial=DeleteMaterial;testRemixApi=&recording;
  auto material=Material(1,0);Mesh(1,material,100);
  frameId=330;RetireSurfaceResources();Check(surfaceMaterials.size()==1 && surfaceMeshes.size()==1,"old material retained by a young mesh");
  frameId=449;RetireSurfaceResources();Check(deletions.empty(),"retirement traversal runs every 30 frames");
  frameId=450;RetireSurfaceResources();Check(deletions==std::vector<char>({'m','a'}),"mesh is destroyed before its material");
  Check(surfaceMeshBytes==0 && surfaceMaterials.empty(),"expired cache bytes and entries released");
  material=Material(2,0);Mesh(2,material,0);rejectMesh=true;
  frameId=600;RetireSurfaceResources();Check(surfaceResourceFailures==1 && surfaceMaterials.size()==1 && surfaceMeshBytes==128,"failed mesh deletion keeps resource and material for retry");
  rejectMesh=false;frameId=630;RetireSurfaceResources();Check(surfaceMaterials.empty() && surfaceMeshBytes==0,"mesh retry frees dependencies in order");
  Material(3,0);rejectMaterial=true;frameId=660;RetireSurfaceResources();
  Check(surfaceMaterials.size()==1 && surfaceResourceFailures==2,"failed material deletion retains the owned handle");
  rejectMaterial=false;frameId=690;RetireSurfaceResources();Check(surfaceMaterials.empty(),"material deletion retry succeeds");
  material=Material(4,0);Mesh(4,material,600);Mesh(5,material,1000);
  frameId=1020;RetireSurfaceResources();Check(surfaceMeshes.size()==1 && surfaceMaterials.size()==1,"shared material outlives the first expired mesh");
  frameId=1350;RetireSurfaceResources();Check(surfaceMaterials.empty() && surfaceMeshes.empty(),"shared material freed after the last mesh");
  Material(5,1340);RetireSurfaceResources();Check(surfaceMaterials.size()==1,"young material without a mesh is retained");
  frameId=1680;RetireSurfaceResources();Check(surfaceMaterials.empty(),"orphan material retires even when mesh cache is empty");
  material=Material(6,0xfffffff0u);Mesh(6,material,0xfffffff0u);
  frameId=330;RetireSurfaceResources();Check(surfaceMaterials.empty() && surfaceMeshes.empty(),"unsigned frame wrap preserves lifetime");
  Material(7,0);recording.DestroyMesh=nullptr;frameId=360;RetireSurfaceResources();Check(surfaceMaterials.size()==1,"missing API deletion functions preserve ownership");
  recording.DestroyMesh=DeleteMesh;RetireSurfaceResources();Check(surfaceMaterials.empty(),"API availability recovery retires resources");
  Check(modelMeshes.empty() && modelMaterials.empty() && surfaceMeshBytes==0,"no API resources or tracked bytes remain");
  Check(surfaceMeshDestroys==5 && surfaceMaterialDestroys==7,"successful deletion counters exclude failed attempts");
  RetireSurfaceResources();Check(surfaceMeshDestroys==5 && surfaceMaterialDestroys==7,"empty retirement does not delete twice");
  frameId=361;material=Material(8,frameId);Mesh(8,material,frameId);rejectMesh=true;
  SetPreserveUnlitColor(false);
  Check(surfaceMeshes.size()==1&&surfaceMaterials.size()==1&&surfaceResourceFailures==3,"forced policy retirement preserves failed mesh dependencies");
  rejectMesh=false;frameId=390;RetireSurfaceResources();
  Check(surfaceMeshes.empty()&&surfaceMaterials.empty(),"forced retirement failure remains eligible for the next retry");
  frameId=391;material=Material(9,frameId);Mesh(9,material,frameId);SetPreserveUnlitColor(true);
  Check(surfaceMeshes.empty()&&surfaceMaterials.empty()&&surfaceMeshBytes==0,"policy switch retires young resources immediately between periodic sweeps");
  Check(modelMeshes.empty()&&modelMaterials.empty(),"forced retirements leave no backend references");
  Pressure(recording);
  printf("{\"checks\":%u,\"meshDestroys\":%u,\"materialDestroys\":%u,\"injectedFailures\":%u,\"pressureRequests\":%u,\"pressureMeshDestroys\":%u,\"pressureMaterialDestroys\":%u,\"pressureRejected\":%u,\"remaining\":0,\"status\":\"PASS\"}\n",
    checks,surfaceMeshDestroys,surfaceMaterialDestroys,surfaceResourceFailures,surfacePressureRequests,surfacePressureMeshDestroys,surfacePressureMaterialDestroys,surfacePressureRejected);
}
