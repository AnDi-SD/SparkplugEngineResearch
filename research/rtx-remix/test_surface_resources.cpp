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
static void Check(bool ok,const char* message) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMesh(remixapi_MeshHandle mesh) {
  Check(modelMeshes.count(mesh)==1,"delete an existing mesh exactly once");
  if(rejectMesh)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  modelMeshes.erase(mesh);deletions.push_back('m');return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMaterial(remixapi_MaterialHandle material) {
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
static void Mesh(uint64_t key,remixapi_MaterialHandle material,unsigned seen) {
  auto handle=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(key));
  Check(modelMaterials.count(material)==1,"mesh creation needs a live material");
  modelMeshes.emplace(handle,material);surfaceMeshes.emplace(key,SurfaceMesh{handle,seen,128,material});surfaceMeshBytes+=128;
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
  printf("{\"checks\":%u,\"meshDestroys\":%u,\"materialDestroys\":%u,\"injectedFailures\":%u,\"remaining\":0,\"status\":\"PASS\"}\n",checks,surfaceMeshDestroys,surfaceMaterialDestroys,surfaceResourceFailures);
}
