// Own standalone CPU backend fixture. No proxy, DLL, native pointers or GPU.
#define REMIX_ALLOW_X86
#define REMIX_WINAPI_NO_LIBRARY_LOADER
#include "../../src/runtime/winx_remix_resource_registry.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <type_traits>

using winx_remix::RemixResourceRegistry;
static unsigned checks;
static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
template<unsigned Index> struct Backend {
  static inline remixapi_Interface api{};
  static inline uintptr_t next = 1000 + Index * 1000;
  static inline std::set<remixapi_MaterialHandle> materials;
  static inline std::set<remixapi_MeshHandle> meshes;
  static inline unsigned creates, destroys;
  static remixapi_Interface* Get() { return &api; }
  static remixapi_ErrorCode REMIXAPI_CALL CreateMaterial(
      const remixapi_MaterialInfo* info, remixapi_MaterialHandle* out) {
    Check(info && out && info->hash == 123, "same logical material input in either backend");
    *out = reinterpret_cast<remixapi_MaterialHandle>(++next);
    materials.insert(*out); ++creates; return REMIXAPI_ERROR_CODE_SUCCESS;
  }
  static remixapi_ErrorCode REMIXAPI_CALL DestroyMaterial(remixapi_MaterialHandle handle) {
    Check(meshes.empty(), "backend material outlives its mesh");
    Check(materials.erase(handle) == 1, "material destruction reaches its owning backend");
    ++destroys; return REMIXAPI_ERROR_CODE_SUCCESS;
  }
  static remixapi_ErrorCode REMIXAPI_CALL CreateMesh(
      const remixapi_MeshInfo* info, remixapi_MeshHandle* out) {
    Check(info && out && info->hash == 456 && info->surfaces_count == 1, "unchanged explicit mesh key");
    const auto& surface = info->surfaces_values[0];
    Check(materials.count(surface.material) == 1, "mesh material belongs to same backend");
    Check(surface.vertices_count == 3 && surface.indices_count == 3 &&
      surface.indices_values[0] == 0 && surface.indices_values[2] == 2, "owned triangle payload");
    *out = reinterpret_cast<remixapi_MeshHandle>(++next);
    meshes.insert(*out); ++creates; return REMIXAPI_ERROR_CODE_SUCCESS;
  }
  static remixapi_ErrorCode REMIXAPI_CALL DestroyMesh(remixapi_MeshHandle handle) {
    Check(meshes.erase(handle) == 1, "mesh destruction reaches its owning backend");
    ++destroys; return REMIXAPI_ERROR_CODE_SUCCESS;
  }
  static void Prepare() {
    api.CreateMaterial = &CreateMaterial; api.DestroyMaterial = &DestroyMaterial;
    api.CreateMesh = &CreateMesh; api.DestroyMesh = &DestroyMesh;
  }
};

struct Pair { remixapi_MaterialHandle material{}; remixapi_MeshHandle mesh{}; };
static Pair Create(RemixResourceRegistry& registry) {
  RemixResourceRegistry::Operation operation(registry);
  Check(operation.owned && operation.Stable(), "independent registry operation starts");
  remixapi_MaterialInfo info{}; info.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO; info.hash = 123;
  Pair result; result.material = registry.CreateMaterialOwned(info, operation);
  std::vector<remixapi_HardcodedVertex> vertices(3);
  vertices[0].position[0] = -1; vertices[1].position[0] = 1; vertices[2].position[1] = 1;
  for (auto& vertex : vertices) vertex.normal[2] = 1;
  const std::vector<uint32_t> indices{0, 1, 2};
  result.mesh = registry.CreateGeometryOwned(vertices, indices, result.material, 456, operation);
  Check(result.material && result.mesh, "resource pair created");
  return result;
}

int main() {
  static_assert(!std::is_copy_constructible_v<RemixResourceRegistry>);
  Backend<0>::Prepare(); Backend<1>::Prepare();
  std::recursive_mutex mutexA, mutexB;
  unsigned frameA = 7, frameB = 10;
  RemixResourceRegistry a(mutexA, frameA, &Backend<0>::Get);
  RemixResourceRegistry b(mutexB, frameB, &Backend<1>::Get);
  const auto pairA = Create(a), pairB = Create(b);
  Check(pairA.mesh != pairB.mesh && pairA.material != pairB.material, "separate API tables and handles");
  Check(a.MeshCurrent(pairA.mesh, pairA.material) && !a.MeshCurrent(pairB.mesh), "first registry liveness is local");
  Check(b.MeshCurrent(pairB.mesh, pairB.material) && !b.MeshCurrent(pairA.mesh), "second registry liveness is local");
  Check(a.Stats().meshBytes == b.Stats().meshBytes && a.Stats().meshBytes == 3 * sizeof(remixapi_HardcodedVertex) + 12,
    "byte accounting belongs to each registry");
  {
    RemixResourceRegistry::Operation operation(a);
    remixapi_MaterialInfo info{}; info.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO; info.hash = 123;
    Check(!b.CreateMaterialOwned(info, operation), "operation cannot authorize another registry");
    Check(Backend<1>::creates == 2, "foreign operation makes no API call");
    remixapi_MaterialHandle cached{};
    Check(a.FindMaterialOwned(123, operation, cached) && cached == pairA.material, "cache lookup uses local material owner");
    remixapi_MeshHandle mesh{};
    Check(a.FindMeshOwned(456, operation, mesh) && mesh == pairA.mesh, "cache lookup uses local mesh owner");
    ++frameB;
    Check(operation.Stable(), "other registry frame does not invalidate this operation");
  }
  frameA = 330;
  a.Retire();
  Check(a.Meshes().empty() && a.Materials().empty() && !a.Stats().meshBytes, "first registry periodic retirement completed");
  Check(b.MeshCurrent(pairB.mesh, pairB.material) && Backend<1>::destroys == 0, "other resources survive first retirement");
  b.Retire(true);
  Check(b.Meshes().empty() && b.Materials().empty() && !b.Stats().meshBytes, "second registry forced retirement completed");
  Check(Backend<0>::creates == Backend<0>::destroys && Backend<1>::creates == Backend<1>::destroys,
    "both backends have balanced ownership");
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"registries\":2,\"creates\":%u,\"destroys\":%u,\"gpu\":false,\"gameExecuted\":false}\n",
    checks, Backend<0>::creates + Backend<1>::creates, Backend<0>::destroys + Backend<1>::destroys);
  return 0;
}
