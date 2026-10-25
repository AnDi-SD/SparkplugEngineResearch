// Own recording CPU fixture for the exact extracted production submitExternalDraw.
// All renderer/GPU collaborators below are stand-ins. This does not execute the
// renderer, mod loader, capture/exporter, instance/BVH cache, game or GPU.
#include "rtx_external_asset_identity.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dxvk {
using XXH64_hash_t = std::uint64_t;
using remixapi_MeshHandle = std::uint64_t;
using remixapi_MaterialHandle = std::uint64_t;
template<class T> using Rc = std::shared_ptr<T>;
constexpr XXH64_hash_t kEmptyHash = 0;
constexpr std::uint32_t kInvalidPointInstanceIndex = ~std::uint32_t(0);
constexpr unsigned VK_CULL_MODE_NONE = 0;
constexpr unsigned VK_CULL_MODE_BACK_BIT = 2;
struct Vector3 { float x = 0, y = 0, z = 0; };
struct Vector4 { float x = 0, y = 0, z = 0, w = 1; Vector3 xyz() const { return {x, y, z}; } };
struct Matrix4 {
  Vector4 columns[4]{};
  Vector4& operator[](std::size_t i) { return columns[i]; }
  const Vector4& operator[](std::size_t i) const { return columns[i]; }
};
static Matrix4 operator*(const Matrix4& a, const Matrix4& b) {
  Matrix4 out = a;
  out[3].x += b[3].x; out[3].y += b[3].y; out[3].z += b[3].z;
  return out;
}
struct AxisAlignedBoundingBox {
  int tag = 0;
  bool isValid() const { return tag != 0; }
  void unionWith(const AxisAlignedBoundingBox& other) { if (other.isValid()) tag = other.tag; }
};
struct CategoryFlags { std::uint32_t value = 0; std::uint32_t raw() const { return value; } };
struct Buffer { bool present = false; bool defined() const { return present; } };
struct RasterGeometry {
  ExternalAssetIdentity externalAssetIdentity{};
  remixapi_MaterialHandle externalMaterial = 0;
  std::uint32_t numBonesPerVertex = 0;
  Buffer blendWeightBuffer, blendIndicesBuffer;
  unsigned cullMode = 0;
  AxisAlignedBoundingBox boundingBox{7};
};
struct RtxParticleSystemDesc { unsigned tag = 1; };
enum class MaterialDataType { Opaque, Translucent, RayPortal };
struct OpaqueMaterialData {
  unsigned color = 10, sampler = 20;
  bool colorAuthored = false, samplerAuthored = false;
  unsigned mergeCalls = 0;
  void merge(const OpaqueMaterialData& original) {
    ++mergeCalls;
    if (!colorAuthored) color = original.color;
    if (!samplerAuthored) sampler = original.sampler;
  }
};
struct MaterialData {
  MaterialDataType type = MaterialDataType::Opaque;
  XXH64_hash_t hash = 111;
  OpaqueMaterialData opaque;
  std::optional<RtxParticleSystemDesc> particles;
  MaterialDataType getType() const { return type; }
  XXH64_hash_t getHash() const { return hash; }
  OpaqueMaterialData& getOpaqueMaterialData() { return opaque; }
  const OpaqueMaterialData& getOpaqueMaterialData() const { return opaque; }
  const RtxParticleSystemDesc* getParticleSystemDesc() const { return particles ? &*particles : nullptr; }
};
struct DxvkSampler {};
struct LegacyMaterialData {
  Rc<DxvkSampler> samplers[2];
  XXH64_hash_t hash = 19;
  void setHashOverride(XXH64_hash_t value) { hash = value; }
  XXH64_hash_t getHash() const { return hash; }
  template<class T> MaterialData as() const { return {}; }
};
struct DrawCallTransforms {
  Matrix4 worldToView, viewToProjection, objectToView, objectToWorld;
  std::shared_ptr<const std::vector<Matrix4>> instancesToObject;
};
struct SkinningState { unsigned numBones = 0; };
struct DrawCallState {
  RasterGeometry geometryData;
  LegacyMaterialData materialData;
  DrawCallTransforms transformData;
  SkinningState skinningData;
  CategoryFlags categories{0x81234567u};
  const SkinningState& getSkinningState() const { return skinningData; }
  const CategoryFlags& getCategoryFlags() const { return categories; }
};
struct CameraType { enum Enum { Main }; };
struct ExternalDrawState {
  DrawCallState drawCall;
  remixapi_MeshHandle mesh = 1;
  CameraType::Enum cameraType = CameraType::Main;
  CategoryFlags categories{0x81234567u};
  bool doubleSided = false;
  std::optional<RtxParticleSystemDesc> optionalParticleDesc;
  std::vector<Matrix4> gpuInstancingTransforms;
  XXH64_hash_t identityHash = 0x8877665544332211ull;
  XXH64_hash_t computeExternalDrawIdentityHash() const { return identityHash; }
};
static XXH64_hash_t spatialMapHashForExternalDrawMesh(remixapi_MeshHandle mesh) { return mesh * 17; }
struct MeshReplacement { RasterGeometry data; };
struct AssetReplacement {
  enum Type { eMesh, eLight, eGraph, eNone } type = eMesh;
  MeshReplacement* geometry = nullptr;
  MaterialData* materialData = nullptr;
  bool includeOriginal = false;
  std::optional<RtxParticleSystemDesc> particleSystem;
  std::optional<unsigned> lightData, graphState;
  std::shared_ptr<std::vector<Matrix4>> instancesToObject;
  std::uint32_t pointInstanceIndex = kInvalidPointInstanceIndex;
};
struct ReplacementInstance;
struct RtInstance;
struct PrimInstance {
  enum class Type { Instance };
  RtInstance* value = nullptr;
  PrimInstance() = default;
  PrimInstance(RtInstance* pointer, Type) : value(pointer) {}
  RtInstance* getInstance() const { return value; }
  void* getUntyped() const { return value; }
};
struct ReplacementInstance {
  struct LookupKey {
    XXH64_hash_t identityHash, spatialMapHash, materialHash, vertexPositionHash;
    Vector3 worldPos;
    Matrix4 transform;
  };
  std::vector<PrimInstance> prims;
  PrimInstance root;
  const std::vector<AssetReplacement>* activeReplacements = nullptr;
  unsigned clears = 0, setups = 0;
  std::uint32_t frameLastSeen = 0, categoryFlags = 0;
  bool isSkinned = true;
  AxisAlignedBoundingBox geometryBoundingBox;
  Matrix4 objectToWorld;
  void clear() {
    ++clears; prims.clear(); root = {}; activeReplacements = nullptr; geometryBoundingBox = {};
  }
  void setup(PrimInstance newRoot, std::size_t size, const std::vector<AssetReplacement>* replacements) {
    clear(); ++setups; root = newRoot; prims.resize(size); activeReplacements = replacements;
  }
};
struct PrimInstanceOwner {
  void setReplacementInstance(ReplacementInstance* owner, std::size_t i, RtInstance* instance, PrimInstance::Type type) {
    owner->prims.at(i) = PrimInstance(instance, type);
  }
};
struct RtInstance { PrimInstanceOwner owner; PrimInstanceOwner& getPrimInstanceOwner() { return owner; } };
struct DrawCallTracker {
  ReplacementInstance replacement;
  std::optional<ReplacementInstance::LookupKey> lastKey;
  bool lastCrossTopology = true;
  unsigned calls = 0;
  ReplacementInstance* findOrCreateReplacementInstance(const ReplacementInstance::LookupKey& key, bool crossTopology) {
    ++calls; lastKey = key; lastCrossTopology = crossTopology; return &replacement;
  }
};
struct AssetReplacer {
  std::map<remixapi_MeshHandle, std::vector<RasterGeometry>> meshes;
  std::map<remixapi_MaterialHandle, MaterialData> materials;
  std::map<XXH64_hash_t, std::vector<AssetReplacement>> replacementMeshes;
  std::map<XXH64_hash_t, MaterialData> replacementMaterials;
  std::vector<XXH64_hash_t> meshLookups, materialLookups;
  const std::vector<RasterGeometry>& accessExternalMesh(remixapi_MeshHandle handle) { return meshes.at(handle); }
  const MaterialData* accessExternalMaterial(remixapi_MaterialHandle handle) {
    auto it = materials.find(handle); return it == materials.end() ? nullptr : &it->second;
  }
  std::vector<AssetReplacement>* getReplacementsForMesh(XXH64_hash_t hash) {
    meshLookups.push_back(hash);
    auto it = replacementMeshes.find(hash); return it == replacementMeshes.end() ? nullptr : &it->second;
  }
  MaterialData* getReplacementMaterial(XXH64_hash_t hash) {
    materialLookups.push_back(hash);
    auto it = replacementMaterials.find(hash); return it == replacementMaterials.end() ? nullptr : &it->second;
  }
};
struct RtCamera {
  Matrix4 view, projection;
  Matrix4 getWorldToView() const { return view; }
  Matrix4 getViewToProjection() const { return projection; }
};
struct CameraManager { RtCamera camera; const RtCamera& getCamera(CameraType::Enum) const { return camera; } };
struct Device { std::uint32_t frame = 33; std::uint32_t getCurrentFrameId() const { return frame; } };
class SceneManager;
struct CommonObjects {
  SceneManager* scene = nullptr;
  SceneManager& getSceneManager() { return *scene; }
};
struct DxvkContext { CommonObjects common; CommonObjects* getCommonObjects() { return &common; } };
struct Logger { static unsigned warnings; static void warn(const char*) { ++warnings; } };
unsigned Logger::warnings = 0;
#define ONCE(statement) do { statement; } while(false)

struct ProcessedDraw {
  DrawCallState draw;
  MaterialData material;
  RtInstance* existing = nullptr;
  bool particles = false;
};
class SceneManager {
public:
  Device* m_device = nullptr;
  AssetReplacer* m_pReplacer = nullptr;
  DrawCallTracker m_drawCallTracker;
  CameraManager camera;
  std::vector<XXH64_hash_t> trackedMeshes;
  std::vector<ProcessedDraw> processed;
  std::vector<std::unique_ptr<RtInstance>> ownedInstances;
  unsigned replacementDraws = 0;
  DrawCallState replacementInput;
  MaterialData replacementMaterial;
  CameraManager& getCameraManager() { return camera; }
  Rc<DxvkSampler> getOrCreateExternalSampler() { return std::make_shared<DxvkSampler>(); }
  void trackMeshHash(XXH64_hash_t hash) { trackedMeshes.push_back(hash); }
  RtInstance* processDrawCallState(Rc<DxvkContext>, const DrawCallState& draw, MaterialData material,
                                  RtInstance* existing, const RtxParticleSystemDesc* particles) {
    processed.push_back({draw, material, existing, particles != nullptr});
    if (existing) return existing;
    ownedInstances.push_back(std::make_unique<RtInstance>());
    return ownedInstances.back().get();
  }
  void drawReplacements(Rc<DxvkContext>, const DrawCallState* draw, const std::vector<AssetReplacement>* replacements,
                        MaterialData& material, ReplacementInstance* instance) {
    ++replacementDraws; replacementInput = *draw; replacementMaterial = material;
    instance->activeReplacements = replacements;
    instance->geometryBoundingBox.tag = 99;
    instance->objectToWorld = draw->transformData.objectToWorld;
  }
  void submitExternalDraw(Rc<DxvkContext> ctx, ExternalDrawState&& state);
};
#include "external_draw.inc"
#undef ONCE
} // namespace dxvk

namespace {
using namespace dxvk;
constexpr XXH64_hash_t Mesh = 0xfedcba9876543210ull;
constexpr XXH64_hash_t Material = 0x89abcdef01234567ull;
unsigned checks = 0;
void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Fixture {
  Device device;
  AssetReplacer assets;
  SceneManager scene;
  Rc<DxvkContext> context = std::make_shared<DxvkContext>();
  MeshReplacement replacementGeometry;
  MaterialData replacementMaterial;
  Fixture(bool admitted = true) {
    scene.m_device = &device; scene.m_pReplacer = &assets; context->common.scene = &scene;
    RasterGeometry geometry;
    geometry.externalMaterial = 2;
    geometry.externalAssetIdentity = ExternalAssetIdentity::ForMesh(admitted, 1, false, Mesh, Material);
    assets.meshes[1] = {geometry}; assets.materials[2].opaque = {41, 42, false, false, 0};
    replacementGeometry.data.boundingBox.tag = 88;
    replacementMaterial.opaque = {91, 92, true, false, 0};
  }
  ExternalDrawState State() const {
    ExternalDrawState state; state.drawCall.transformData.objectToWorld[3].x = 5;
    return state;
  }
  void Submit() { scene.submitExternalDraw(context, State()); }
  ReplacementInstance& Instance() { return scene.m_drawCallTracker.replacement; }
  void MeshMod() {
    AssetReplacement replacement; replacement.geometry = &replacementGeometry;
    assets.replacementMeshes[Mesh] = {replacement};
  }
  void CheckStableLookups() {
    Check(!assets.meshLookups.empty() && assets.meshLookups.back() == Mesh, "mesh lookup receives full stable capture identity");
    Check(!assets.materialLookups.empty() && assets.materialLookups.back() == Material, "material lookup receives full stable capture identity");
    Check(!scene.trackedMeshes.empty() && scene.trackedMeshes.back() == Mesh, "hash tracking uses the same stable mesh identity");
  }
};
void DefaultAndStableFallback() {
  for (bool admitted : {false, true}) {
    Fixture f(admitted); const auto expected = f.State(); f.Submit();
    Check(f.scene.processed.size() == 1 && f.scene.replacementDraws == 0, "unreplaced draw submits original geometry exactly once");
    const auto& draw = f.scene.processed[0].draw;
    Check(draw.geometryData.externalAssetIdentity.meshHash == (admitted ? Mesh : 0), "default-off does not opt into stable asset identity");
    Check(draw.materialData.getHash() == f.assets.materials[2].hash, "stable asset material ID never replaces the legacy external material content hash");
    Check(f.scene.processed[0].material.opaque.color == 41 && f.scene.processed[0].material.opaque.sampler == 42, "fallback preserves actual external material values");
    Check(draw.geometryData.cullMode == VK_CULL_MODE_BACK_BIT, "default external cull state is preserved");
    Check(draw.materialData.samplers[0] && draw.materialData.samplers[1], "external default samplers are installed");
    Check(f.Instance().frameLastSeen == f.device.frame, "frame bookkeeping survives fallback");
    Check(f.Instance().geometryBoundingBox.tag == 7 && f.Instance().objectToWorld[3].x == 5, "fallback retains source bounds and transform");
    Check(f.scene.m_drawCallTracker.calls == 1 && !f.scene.m_drawCallTracker.lastCrossTopology, "one precise external instance lookup disallows indirect topology matching");
    const auto& key = *f.scene.m_drawCallTracker.lastKey;
    Check(key.identityHash == expected.identityHash && key.spatialMapHash == spatialMapHashForExternalDrawMesh(expected.mesh), "stable assets do not replace external draw or spatial keys");
    Check(key.materialHash == expected.drawCall.materialData.getHash() && key.vertexPositionHash == kEmptyHash, "tracker key preserves pre-surface external material and vertex-position conventions");
    Check(key.worldPos.x == 5 && key.transform[3].x == 5, "tracker retains source world position and transform");
    if (admitted) {
      f.CheckStableLookups();
      Check(f.Instance().categoryFlags == draw.categories.raw() && !f.Instance().isSkinned, "admitted rigid bookkeeping records source categories and rigid state");
    } else {
      Check(f.assets.meshLookups.empty() && f.assets.materialLookups.empty() && f.scene.trackedMeshes.empty(), "default callers bypass all new mod/hash lookups");
    }
  }
}
void MaterialMods() {
  Fixture f; f.assets.replacementMaterials[Material] = f.replacementMaterial; f.Submit();
  f.CheckStableLookups();
  Check(f.scene.processed[0].material.opaque.color == 91 && f.scene.processed[0].material.opaque.sampler == 42, "authored material values merge with actual external fallback values");
  Check(f.scene.processed[0].material.opaque.mergeCalls == 1, "external Opaque material merge is invoked");
  Check(f.assets.replacementMaterials[Material].opaque.sampler == 92 && f.assets.replacementMaterials[Material].opaque.mergeCalls == 0, "mod material is copied before merging");
  Check(f.assets.materials[2].opaque.color == 41 && f.assets.materials[2].opaque.mergeCalls == 0, "original external material is immutable");
  for (auto type : {MaterialDataType::Translucent, MaterialDataType::RayPortal}) {
    Fixture bad; bad.assets.replacementMaterials[Material].type = type;
    const auto warnings = Logger::warnings; bad.Submit();
    Check(bad.scene.processed.size() == 1 && bad.scene.processed[0].material.opaque.color == 41, "unsupported material type falls back to original external Opaque");
    Check(Logger::warnings > warnings, "unsupported material type has an explicit diagnostic");
  }
  Fixture emitter;
  emitter.assets.replacementMaterials[Material] = emitter.replacementMaterial;
  emitter.assets.replacementMaterials[Material].particles.emplace();
  const auto warnings = Logger::warnings; emitter.Submit();
  Check(emitter.scene.processed.size() == 1 && emitter.scene.processed[0].material.opaque.color == 41 &&
        emitter.scene.processed[0].material.getParticleSystemDesc() == nullptr,
        "Opaque material mod with implicit particle system falls back to source material");
  Check(Logger::warnings > warnings, "implicit material emitter receives an explicit diagnostic");
}
void MeshModsAndRemoval() {
  Fixture f; f.MeshMod(); f.Submit(); f.CheckStableLookups();
  Check(f.scene.replacementDraws == 1 && f.scene.processed.empty(), "supported mesh replacement delegates to stock replacement drawing");
  Check(f.scene.replacementInput.geometryData.externalAssetIdentity.meshHash == Mesh &&
        f.scene.replacementInput.geometryData.externalAssetIdentity.materialHash == Material &&
        f.scene.replacementInput.materialData.getHash() == f.assets.materials[2].hash,
        "replacement delegate receives stable source IDs and preserves the separate content hash");
  Check(f.Instance().activeReplacements == &f.assets.replacementMeshes[Mesh], "mesh replacement tracks the mod vector on the external instance");
  Check(f.Instance().geometryBoundingBox.tag == 99, "replacement-computed bounds are not overwritten by original mesh bounds");
  Check(f.Instance().frameLastSeen == f.device.frame && !f.Instance().isSkinned && f.Instance().categoryFlags == 0x81234567u, "replacement bookkeeping retains source frame and categories");
  f.assets.replacementMeshes.erase(Mesh); const auto clears = f.Instance().clears;
  ++f.device.frame; f.Submit();
  Check(f.scene.replacementDraws == 1 && f.scene.processed.size() == 1, "removing mesh mod returns to one original draw");
  Check(f.Instance().clears > clears && f.Instance().activeReplacements == nullptr, "removed mod detaches previous replacement state");
  Check(f.scene.processed.back().existing == nullptr && f.Instance().geometryBoundingBox.tag == 7, "fallback does not reuse stale mod instance or bounds");

  Fixture original; original.MeshMod(); original.assets.replacementMeshes[Mesh][0].includeOriginal = true;
  original.assets.replacementMeshes[Mesh][0].type = AssetReplacement::eNone;
  original.assets.replacementMeshes[Mesh][0].geometry = nullptr;
  original.Submit();
  Check(original.scene.replacementDraws == 1 && original.scene.processed.empty(), "single include-original entry is supported without a replacement geometry");
  Check(original.scene.replacementInput.geometryData.externalMaterial == 2, "include-original keeps source external material capture witness");
}
void HideAndUnhide() {
  Fixture f; f.Submit(); Check(f.Instance().root.getUntyped() != nullptr, "hide test first owns original instance");
  f.assets.replacementMeshes[Mesh] = {}; const auto draws = f.scene.processed.size(); const auto clears = f.Instance().clears;
  f.Submit();
  Check(f.scene.processed.size() == draws && f.scene.replacementDraws == 0, "empty authored replacement hides without original fallback");
  Check(f.Instance().clears > clears && f.Instance().root.getUntyped() == nullptr && f.Instance().prims.empty(), "hide removes previously visible prims");
  Check(f.Instance().activeReplacements == &f.assets.replacementMeshes[Mesh] && !f.Instance().geometryBoundingBox.isValid(), "hide retains authoring state with invalid visible bounds");
  Check(f.Instance().frameLastSeen == f.device.frame && f.Instance().categoryFlags == 0x81234567u && !f.Instance().isSkinned, "hidden instance still records frame and source category state");
  f.assets.replacementMeshes.erase(Mesh); f.Submit();
  Check(f.scene.processed.size() == draws + 1 && f.Instance().root.getUntyped() != nullptr, "removing empty replacement makes source geometry visible again");
  Check(f.Instance().activeReplacements == nullptr && f.Instance().geometryBoundingBox.tag == 7, "unhide restores original ownership and bounds");
}
void UnsupportedMeshMods() {
  for (unsigned mode = 0; mode < 15; ++mode) {
    Fixture f; f.MeshMod(); f.Submit();
    auto& mods = f.assets.replacementMeshes[Mesh]; auto& mod = mods[0];
    switch (mode) {
    case 0: mods.push_back(mod); break;
    case 1: mod.particleSystem.emplace(); break;
    case 2: mod.type = AssetReplacement::eLight; break;
    case 3: mod.type = AssetReplacement::eGraph; break;
    case 4: mod.lightData = 1; break;
    case 5: mod.graphState = 1; break;
    case 6: mod.instancesToObject = std::make_shared<std::vector<Matrix4>>(1); break;
    case 7: mod.pointInstanceIndex = 3; break;
    case 8: mod.geometry = nullptr; break;
    case 9: f.replacementGeometry.data.numBonesPerVertex = 4; break;
    case 10: f.replacementGeometry.data.blendWeightBuffer.present = true; break;
    case 11: f.replacementGeometry.data.blendIndicesBuffer.present = true; break;
    case 12: f.replacementMaterial.type = MaterialDataType::Translucent; mod.materialData = &f.replacementMaterial; break;
    case 13: mod.type = AssetReplacement::eNone; break;
    case 14: f.replacementMaterial.particles.emplace(); mod.materialData = &f.replacementMaterial; break;
    }
    const auto clears = f.Instance().clears; const auto warnings = Logger::warnings; f.Submit();
    Check(f.scene.replacementDraws == 1 && f.scene.processed.size() == 1, "unsupported mesh mod falls back to exactly one original draw");
    Check(f.Instance().clears > clears && f.Instance().activeReplacements == nullptr, "unsupported mod detaches previously active replacement");
    Check(f.scene.processed.back().existing == nullptr && f.scene.processed.back().draw.geometryData.externalAssetIdentity.meshHash == Mesh, "unsupported mod fallback uses source geometry and stable source capture identity");
    Check(Logger::warnings > warnings, "unsupported mod shape has an explicit diagnostic");
  }
}
void UnsupportedSourceDraws() {
  for (unsigned mode = 0; mode < 11; ++mode) {
    Fixture f; f.MeshMod(); f.Submit();
    const auto meshLookups = f.assets.meshLookups.size(), materialLookups = f.assets.materialLookups.size();
    auto state = f.State(); auto& source = f.assets.meshes[1][0];
    switch (mode) {
    case 0: f.assets.materials.clear(); break;
    case 1: f.assets.materials[2].type = MaterialDataType::Translucent; break;
    case 2: state.drawCall.skinningData.numBones = 1; break;
    case 3: source.numBonesPerVertex = 4; break;
    case 4: source.blendWeightBuffer.present = true; break;
    case 5: source.blendIndicesBuffer.present = true; break;
    case 6: state.optionalParticleDesc.emplace(); break;
    case 7: state.gpuInstancingTransforms.resize(1); break;
    case 8: f.assets.meshes[1].push_back(source); break;
    case 9: source.externalAssetIdentity.materialHash = 0; break;
    case 10: f.assets.materials[2].particles.emplace(); break;
    }
    const auto clears = f.Instance().clears; f.scene.submitExternalDraw(f.context, std::move(state));
    Check(f.assets.meshLookups.size() == meshLookups && f.assets.materialLookups.size() == materialLookups, "unsupported source draw bypasses stable replacement lookup");
    Check(f.scene.replacementDraws == 1 && f.scene.processed.size() == (mode == 8 ? 2u : 1u), "unsupported source draw returns to original external submission count");
    Check(f.Instance().clears > clears && f.Instance().activeReplacements == nullptr, "losing source admission clears old active replacement");
    for (const auto& processed : f.scene.processed) {
      Check(processed.draw.geometryData.externalAssetIdentity.meshHash == 0 && processed.draw.geometryData.externalAssetIdentity.materialHash == 0, "unsupported source clears both stable capture identities");
    }
    if (mode == 6) Check(f.scene.processed.back().particles, "default particle descriptor survives unsupported source fallback");
    if (mode == 7) Check(f.scene.processed.back().draw.transformData.instancesToObject && f.scene.processed.back().draw.transformData.instancesToObject->size() == 1, "moved GPU instancing transforms stay present on original fallback");
  }
}
void StableLookupUnderChangedPlacementAndHandles() {
  Fixture f; f.Submit();
  const auto oldMeshLookup = f.assets.meshLookups.back(), oldMaterialLookup = f.assets.materialLookups.back();
  f.assets.meshes[0x4321] = f.assets.meshes[1]; f.assets.meshes[0x4321][0].externalMaterial = 0x1234;
  f.assets.materials[0x1234] = f.assets.materials[2];
  f.assets.materials[0x1234].hash = 0xffffeeee11112222ull;
  auto state = f.State(); state.mesh = 0x4321; state.drawCall.transformData.objectToWorld[3].x = 100;
  state.identityHash ^= 0x11; state.doubleSided = true; f.scene.submitExternalDraw(f.context, std::move(state));
  Check(f.assets.meshLookups.back() == oldMeshLookup && f.assets.materialLookups.back() == oldMaterialLookup, "stable asset lookup survives source placement and external handle changes");
  Check(f.scene.processed.back().draw.geometryData.cullMode == VK_CULL_MODE_NONE, "double-sided external cull behavior remains intact");
  Check(f.scene.m_drawCallTracker.lastKey->spatialMapHash == spatialMapHashForExternalDrawMesh(0x4321), "external lifetime/spatial handle remains independent of content identity");
  Check(f.scene.m_drawCallTracker.lastKey->worldPos.x == 100 && f.scene.m_drawCallTracker.lastKey->identityHash == (0x8877665544332211ull ^ 0x11), "changed instance placement and precise draw identity still reach external tracker");
}
} // namespace

int main() {
  DefaultAndStableFallback(); MaterialMods(); MeshModsAndRemoval(); HideAndUnhide();
  UnsupportedMeshMods(); UnsupportedSourceDraws(); StableLookupUnderChangedPlacementAndHandles();
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"rendererExecuted\":false,\"gpuExecuted\":false,\"scope\":\"extracted production submitExternalDraw with recording CPU collaborators\"}\n", checks);
  return 0;
}
