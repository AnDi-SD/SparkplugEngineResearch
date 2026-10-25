// Own recording fixture for actual extracted renderer capture functions.
// Compile after prepare_capture_fixture.py with its output directory and the
// patched renderer's src/dxvk/rtx_render on the include path. No USD/GPU used.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "rtx_external_asset_identity.h"

static unsigned checks = 0;
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { \
  std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #__VA_ARGS__); \
  return false; } } while (false)

namespace dxvk {
using XXH64_hash_t = uint64_t;
constexpr XXH64_hash_t kEmptyHash = 0;
using HashRule = uint32_t;
using CategoryFlags = uint32_t;
using remixapi_MaterialHandle = uint64_t;
template<class T> using Rc = std::shared_ptr<T>;
struct DxvkContext {};
struct RtxOptions {
  static const HashRule& geometryAssetHashRule() { static const HashRule value = 7; return value; }
};
static std::string hashToString(const uint64_t value) {
  std::ostringstream out;
  out << std::hex << value;
  return out.str();
}
struct Logger {
  inline static unsigned warnings = 0;
  static void warn(const std::string&) { ++warnings; }
  static void debug(const std::string&) {}
};
namespace str {
template<class A, class B> std::string format(const A& first, const B& second) {
  return std::string(first) + std::string(second);
}
}
namespace lss {
namespace ext { static const std::string dds = ".dds"; }
namespace commonDirName { static const std::string texDir = "textures/"; }
struct Material {
  std::string matName;
  std::string albedoTexPath;
  bool enableOpacity = false;
  bool capturedExternalOpaque = false;
  uint64_t opaqueMarker = 0;
  struct Sampler {
    int addrModeU = 0;
    int addrModeV = 0;
    int filter = 0;
    int borderColor = 0;
  } sampler;
};
}
static const std::string BASE_DIR = "capture/";
struct ImageView { int marker = 17; int image() const { return marker; } };
struct TextureRef { const ImageView* view = nullptr; const ImageView* getImageView() const { return view; } };
struct SamplerInfo { int addressModeU = 1, addressModeV = 2, magFilter = 3, borderColor = 4; };
struct Sampler { SamplerInfo data; const SamplerInfo& info() const { return data; } };
struct LegacyMaterialData {
  uint64_t hash = 0;
  TextureRef color;
  const Sampler* sampler = nullptr;
  uint64_t getHash() const { return hash; }
  const TextureRef& getColorTexture() const { return color; }
  const Sampler* getSampler() const { return sampler; }
};
struct RasterGeometry {
  uint64_t componentHash = 0;
  remixapi_MaterialHandle externalMaterial = 0;
  ExternalAssetIdentity externalAssetIdentity;
  uint64_t getHashForRule(const HashRule& rule) const { return componentHash ^ rule; }
};
struct DrawCallState {
  RasterGeometry geometryData;
  LegacyMaterialData materialData;
  #include "capture_draw_members.inc"
  const RasterGeometry& getGeometryData() const { return geometryData; }
  const LegacyMaterialData& getMaterialData() const { return materialData; }
};
struct BlasEntry {
  DrawCallState input;
  std::map<uint64_t, LegacyMaterialData> contentMaterials;
  mutable std::vector<uint64_t> requestedContentKeys;
  const LegacyMaterialData& getMaterialData(uint64_t key) const {
    requestedContentKeys.push_back(key);
    const auto found = contentMaterials.find(key);
    if (found == contentMaterials.end()) throw std::runtime_error("Capture used asset ID as BLAS content key");
    return found->second;
  }
};
struct RtInstance {
  const BlasEntry* blas = nullptr;
  uint64_t id = 0;
  uint64_t m_materialDataHash = 0;
  uint64_t m_captureMaterialHash = 0;
  uint64_t m_captureMeshAssetHash = 0;
  bool isFrontFaceFlipped = false;
  struct Surface { struct AlphaState { bool isFullyOpaque = true; } alphaState; } surface;
  RtInstance() = default;
  RtInstance(const RtInstance& src) : blas(src.blas), id(src.id), isFrontFaceFlipped(src.isFrontFaceFlipped), surface(src.surface) {
    #include "capture_copy.inc"
  }
  #include "capture_instance_members.inc"
  const BlasEntry* getBlas() const { return blas; }
  uint64_t getId() const { return id; }
  CategoryFlags getCategoryFlags() const { return 3; }
  void updateFrom(const DrawCallState& drawCall) {
    RtInstance& currentInstance = *this;
    #include "capture_assign.inc"
  }
};
enum class MaterialDataType { Opaque, Other };
struct OpaqueMaterialData { uint64_t marker = 0; };
struct MaterialData {
  MaterialDataType type = MaterialDataType::Opaque;
  OpaqueMaterialData opaque;
  MaterialDataType getType() const { return type; }
  const OpaqueMaterialData& getOpaqueMaterialData() const { return opaque; }
};
struct AssetReplacer {
  std::map<remixapi_MaterialHandle, MaterialData> materials;
  std::vector<remixapi_MaterialHandle> requestedHandles;
  const MaterialData* accessExternalMaterial(remixapi_MaterialHandle handle) {
    requestedHandles.push_back(handle);
    auto found = materials.find(handle);
    return found == materials.end() ? nullptr : &found->second;
  }
};
struct SceneManager {
  AssetReplacer assets;
  AssetReplacer* getAssetReplacer() { return &assets; }
};
struct AssetExporter {
  struct Dump { std::string directory, filename; int image; };
  struct OpaqueCall { uint64_t hash, marker; bool opacity; };
  std::vector<Dump> dumps;
  std::vector<OpaqueCall> opaqueCalls;
  void dumpImageToFile(const Rc<DxvkContext>&, const std::string& directory, const std::string& filename, const int image) {
    dumps.push_back({ directory, filename, image });
  }
};
static lss::Material captureExternalOpaqueMaterial(const Rc<DxvkContext>&,
    AssetExporter& exporter, const OpaqueMaterialData& material,
    const XXH64_hash_t materialHash, const bool enableOpacity) {
  exporter.opaqueCalls.push_back({ materialHash, material.marker, enableOpacity });
  lss::Material result;
  result.matName = hashToString(materialHash);
  result.enableOpacity = enableOpacity;
  result.capturedExternalOpaque = true;
  result.opaqueMarker = material.marker;
  return result;
}
struct GameCapturer {
  struct Mesh { size_t instanceCount = 0; uint64_t matHash = 0; };
  struct Instance {
    uint64_t meshHash = 0, matHash = 0;
    size_t meshInstNum = 0;
    struct Lss { float firstTime = 0; } lssData;
  };
  struct Material { lss::Material lssData; };
  struct Capture {
    std::map<uint64_t, Material> materials;
    std::map<uint64_t, std::shared_ptr<Mesh>> meshes;
    std::map<uint64_t, Instance> instances;
    float currentFrameNum = 0;
    std::string idStr = "fixture";
  };
  struct MeshCall { uint64_t hash; CategoryFlags flags; bool leftHanded; };
  std::unique_ptr<Capture> m_pCap = std::make_unique<Capture>();
  std::mutex m_meshMutex;
  SceneManager m_sceneManager;
  AssetExporter m_exporter;
  std::vector<MeshCall> meshCalls;
  void newInstance(const Rc<DxvkContext> ctx, const RtInstance& rtInstance);
  void captureMaterial(const Rc<DxvkContext> ctx, const LegacyMaterialData& materialData,
                       const bool bEnableOpacity, const XXH64_hash_t captureHash);
  void captureMesh(const Rc<DxvkContext>&, uint64_t hash, const BlasEntry&, CategoryFlags flags,
                   bool first, bool positions, bool normals, bool indices, bool leftHanded) {
    if (!first || !positions || !normals || !indices) throw std::runtime_error("Unexpected capture flags");
    meshCalls.push_back({ hash, flags, leftHanded });
  }
};
#include "capture_functions.inc"

static BlasEntry makeBlas(uint64_t meshId, uint64_t materialId, uint64_t component, uint64_t content,
                          bool enabled = true) {
  BlasEntry result;
  result.input.geometryData.componentHash = component;
  result.input.geometryData.externalMaterial = materialId;
  result.input.geometryData.externalAssetIdentity = ExternalAssetIdentity::ForMesh(enabled, 1, false, meshId, materialId);
  result.input.materialData.hash = content;
  result.contentMaterials.emplace(content, result.input.materialData);
  return result;
}
static RtInstance makeInstance(const BlasEntry& blas, uint64_t id, bool fullyOpaque = true) {
  RtInstance result;
  result.blas = &blas;
  result.id = id;
  result.surface.alphaState.isFullyOpaque = fullyOpaque;
  result.updateFrom(blas.input);
  return result;
}
static void installOpaque(GameCapturer& capture, uint64_t id, uint64_t marker = 91) {
  capture.m_sceneManager.assets.materials.emplace(id, MaterialData { MaterialDataType::Opaque, { marker } });
}

static bool stableCaptureAndCopy() {
  constexpr uint64_t meshId = UINT64_C(0xf123456789abcdef);
  constexpr uint64_t materialId = UINT64_C(0xe23456789abcdef1);
  auto firstBlas = makeBlas(meshId, materialId, 101, 201);
  auto secondBlas = makeBlas(meshId, materialId, 500, 202);
  auto first = makeInstance(firstBlas, 501, false);
  RtInstance copied(first);
  CHECK(copied.getMaterialDataHash() == 201);
  CHECK(copied.getCaptureMaterialHash() == materialId);
  CHECK(copied.getCaptureMeshAssetHash() == meshId);
  CHECK(copied.getBlas() == &firstBlas);
  CHECK(firstBlas.input.getHash(RtxOptions::geometryAssetHashRule()) != secondBlas.input.getHash(RtxOptions::geometryAssetHashRule()));
  CHECK(firstBlas.input.getAssetHash(RtxOptions::geometryAssetHashRule()) == meshId);
  CHECK(secondBlas.input.getAssetHash(RtxOptions::geometryAssetHashRule()) == meshId);
  GameCapturer capture;
  installOpaque(capture, materialId);
  const auto ctx = std::make_shared<DxvkContext>();
  capture.newInstance(ctx, copied);
  CHECK(firstBlas.requestedContentKeys == std::vector<uint64_t> { 201 });
  CHECK(capture.m_pCap->materials.size() == 1);
  CHECK(capture.m_pCap->materials.count(materialId) == 1);
  CHECK(capture.m_pCap->materials.count(201) == 0);
  CHECK(capture.m_exporter.opaqueCalls.size() == 1);
  CHECK(capture.m_exporter.opaqueCalls[0].hash == materialId);
  CHECK(capture.m_exporter.opaqueCalls[0].opacity);
  CHECK(capture.m_exporter.opaqueCalls[0].marker == 91);
  CHECK(capture.m_pCap->materials.at(materialId).lssData.matName == hashToString(materialId));
  CHECK(capture.m_pCap->meshes.at(meshId)->matHash == materialId);
  CHECK(capture.m_pCap->instances.at(501).meshHash == meshId);
  CHECK(capture.m_pCap->instances.at(501).matHash == materialId);
  CHECK(capture.meshCalls.size() == 1 && capture.meshCalls[0].hash == meshId);
  CHECK(capture.meshCalls[0].flags == 3);
  CHECK(!capture.meshCalls[0].leftHanded);

  // New instance at a later capture frame, after the external table retires.
  // Capture already owns its material values and uses stable map keys.
  capture.m_pCap->currentFrameNum = 27;
  capture.m_sceneManager.assets.materials.clear();
  auto second = makeInstance(secondBlas, 999);
  capture.newInstance(ctx, second);
  CHECK(secondBlas.requestedContentKeys == std::vector<uint64_t> { 202 });
  CHECK(capture.m_pCap->materials.size() == 1);
  CHECK(capture.m_pCap->meshes.size() == 1);
  CHECK(capture.m_pCap->meshes.at(meshId)->instanceCount == 2);
  CHECK(capture.m_pCap->instances.at(999).meshInstNum == 1);
  CHECK(capture.m_pCap->instances.at(999).lssData.firstTime == 27);
  CHECK(capture.m_pCap->instances.at(501).lssData.firstTime == 0);
  CHECK(capture.m_pCap->materials.at(materialId).lssData.opaqueMarker == 91);
  CHECK(capture.m_exporter.opaqueCalls.size() == 1);
  CHECK(capture.meshCalls.size() == 1);
  CHECK(capture.m_sceneManager.assets.requestedHandles == std::vector<uint64_t> { materialId });
  return true;
}

static bool fallbackNaming() {
  constexpr uint64_t meshId = UINT64_C(0xa123456789abcdef);
  constexpr uint64_t materialId = UINT64_C(0xb23456789abcdef1);
  ImageView image;
  Sampler sampler;
  const auto ctx = std::make_shared<DxvkContext>();
  for (bool unsupported : { false, true }) {
    auto blas = makeBlas(meshId, materialId, 411, 511);
    blas.contentMaterials.at(511).color.view = &image;
    blas.contentMaterials.at(511).sampler = &sampler;
    auto instance = makeInstance(blas, unsupported ? 32 : 31, false);
    GameCapturer capture;
    if (unsupported) capture.m_sceneManager.assets.materials.emplace(materialId, MaterialData { MaterialDataType::Other, {} });
    const auto warningCount = Logger::warnings;
    capture.newInstance(ctx, instance);
    CHECK(capture.m_pCap->materials.size() == 1);
    CHECK(capture.m_pCap->materials.count(materialId) == 1);
    CHECK(capture.m_pCap->materials.count(511) == 0);
    const auto& material = capture.m_pCap->materials.at(materialId).lssData;
    CHECK(material.matName == hashToString(materialId));
    CHECK(material.enableOpacity);
    CHECK(!material.capturedExternalOpaque);
    CHECK(material.albedoTexPath == "capture/textures/" + hashToString(materialId) + ".dds");
    CHECK(material.sampler.addrModeU == 1 && material.sampler.addrModeV == 2);
    CHECK(material.sampler.filter == 3 && material.sampler.borderColor == 4);
    CHECK(capture.m_exporter.dumps.size() == 1);
    CHECK(capture.m_exporter.dumps[0].filename == hashToString(materialId) + ".dds");
    CHECK(capture.m_exporter.dumps[0].directory == "capture/textures/");
    CHECK(capture.m_exporter.dumps[0].image == 17);
    CHECK(capture.m_exporter.opaqueCalls.empty());
    CHECK(Logger::warnings == warningCount + (unsupported ? 1u : 0u));
    CHECK(blas.requestedContentKeys == std::vector<uint64_t> { 511 });
    capture.m_sceneManager.assets.materials.clear();
    image.marker = 55;
    CHECK(material.matName == hashToString(materialId));
    CHECK(material.albedoTexPath == "capture/textures/" + hashToString(materialId) + ".dds");
    image.marker = 17;
  }
  // Null legacy texture/sampler is legal and must still use the supplied name.
  GameCapturer direct;
  LegacyMaterialData legacy;
  legacy.hash = 711;
  direct.captureMaterial(ctx, legacy, false, materialId);
  CHECK(direct.m_pCap->materials.count(materialId) == 1);
  CHECK(direct.m_pCap->materials.count(711) == 0);
  CHECK(direct.m_pCap->materials.at(materialId).lssData.albedoTexPath.empty());
  CHECK(direct.m_exporter.dumps.empty());
  return true;
}

static bool legacyAndUnsupportedAdmission() {
  const auto ctx = std::make_shared<DxvkContext>();
  auto blas = makeBlas(901, 902, 903, 904, false);
  auto instance = makeInstance(blas, 905);
  CHECK(instance.getCaptureMaterialHash() == 904);
  CHECK(instance.getCaptureMeshAssetHash() == 0);
  CHECK(instance.getMaterialDataHash() == 904);
  CHECK(blas.input.getAssetHash(RtxOptions::geometryAssetHashRule()) == blas.input.getHash(RtxOptions::geometryAssetHashRule()));
  GameCapturer capture;
  installOpaque(capture, 902);
  capture.newInstance(ctx, instance);
  CHECK(capture.m_pCap->materials.count(904) == 1);
  CHECK(capture.m_pCap->materials.count(902) == 0);
  CHECK(capture.m_pCap->meshes.count(blas.input.getHash(RtxOptions::geometryAssetHashRule())) == 1);
  CHECK(capture.m_exporter.opaqueCalls[0].hash == 904);
  CHECK(capture.m_pCap->instances.at(905).matHash == 904);
  for (const auto identity : { ExternalAssetIdentity { 0, 123 }, ExternalAssetIdentity { 456, 0 } }) {
    blas.input.geometryData.externalAssetIdentity = identity;
    instance.updateFrom(blas.input);
    CHECK(instance.getCaptureMaterialHash() == 904);
    CHECK(instance.getCaptureMeshAssetHash() == 0);
    CHECK(blas.input.getAssetHash(RtxOptions::geometryAssetHashRule()) == blas.input.getHash(RtxOptions::geometryAssetHashRule()));
  }
  // The real SceneManager supplies these draw-admission inputs. Unsupported
  // callers retain the old export naming; the fixture does not emulate it.
  for (unsigned mode = 0; mode != 4; ++mode) {
    blas.input.geometryData.externalAssetIdentity = ExternalAssetIdentity::ForMesh(true, 1, false, 901, 902)
      .ForDraw(mode != 0, mode == 1, mode == 2, mode == 3);
    instance.updateFrom(blas.input);
    CHECK(instance.getCaptureMaterialHash() == 904);
    CHECK(instance.getCaptureMeshAssetHash() == 0);
    CHECK(blas.input.getAssetHash(RtxOptions::geometryAssetHashRule()) == blas.input.getHash(RtxOptions::geometryAssetHashRule()));
  }
  return true;
}

static bool restartedCreationOrder() {
  const auto ctx = std::make_shared<DxvkContext>();
  std::map<uint64_t, std::pair<uint64_t, std::string>> names[2];
  for (unsigned run = 0; run != 2; ++run) {
    GameCapturer capture;
    for (unsigned ordinal = 0; ordinal != 3; ++ordinal) {
      const uint64_t item = run == 0 ? ordinal : 2 - ordinal;
      const uint64_t meshId = UINT64_C(0xfeed123400000000) + item;
      const uint64_t materialId = UINT64_C(0xface567800000000) + item;
      auto blas = makeBlas(meshId, materialId, 1000 + 11 * run + ordinal, 2000 + 11 * run + ordinal);
      auto instance = makeInstance(blas, 3000 + 11 * run + ordinal);
      installOpaque(capture, materialId, 77 + item);
      capture.newInstance(ctx, instance);
      CHECK(blas.requestedContentKeys == std::vector<uint64_t> { 2000 + 11 * run + ordinal });
      CHECK(capture.m_pCap->instances.at(instance.id).meshHash == meshId);
      CHECK(capture.m_pCap->instances.at(instance.id).matHash == materialId);
    }
    CHECK(capture.m_pCap->meshes.size() == 3);
    CHECK(capture.m_pCap->materials.size() == 3);
    CHECK(capture.m_exporter.opaqueCalls.size() == 3);
    for (const auto& item : capture.m_pCap->meshes) {
      const auto material = item.second->matHash;
      names[run].emplace(item.first, std::make_pair(material, capture.m_pCap->materials.at(material).lssData.matName));
    }
  }
  CHECK(names[0] == names[1]);
  CHECK(names[0].size() == 3);
  return true;
}

static bool sharedBlasAdmissionIsPerInstance() {
  constexpr uint64_t meshId = UINT64_C(0xdefa123456789abc);
  constexpr uint64_t materialId = UINT64_C(0xcafe123456789abc);
  const auto ctx = std::make_shared<DxvkContext>();
  for (bool cachedAdmitted : { false, true }) {
    for (bool captureAdmittedFirst : { false, true }) {
      // The real same-frame cache hit keeps the first draw's BLAS input.
      auto blas = makeBlas(meshId, materialId, 750, 977, cachedAdmitted);
      const auto oldMeshName = blas.input.getHash(RtxOptions::geometryAssetHashRule());
      CHECK(oldMeshName != meshId);
      DrawCallState admitted = blas.input;
      admitted.geometryData.externalAssetIdentity = ExternalAssetIdentity::ForMesh(true, 1, false, meshId, materialId)
        .ForDraw(true, false, false, false);
      DrawCallState excluded = admitted;
      excluded.geometryData.externalAssetIdentity = admitted.geometryData.externalAssetIdentity.ForDraw(true, false, true, false);
      auto ordinary = makeInstance(blas, 701);
      auto particle = makeInstance(blas, 702);
      ordinary.updateFrom(admitted);
      particle.updateFrom(excluded);
      CHECK(ordinary.getBlas() == particle.getBlas());
      CHECK(ordinary.getCaptureMeshAssetHash() == meshId);
      CHECK(ordinary.getCaptureMaterialHash() == materialId);
      CHECK(particle.getCaptureMeshAssetHash() == 0);
      CHECK(particle.getCaptureMaterialHash() == 977);
      CHECK(ordinary.getMaterialDataHash() == 977 && particle.getMaterialDataHash() == 977);
      RtInstance copied(ordinary);
      CHECK(copied.getCaptureMeshAssetHash() == meshId && copied.getBlas() == &blas);
      GameCapturer capture;
      installOpaque(capture, materialId);
      capture.newInstance(ctx, captureAdmittedFirst ? copied : particle);
      capture.newInstance(ctx, captureAdmittedFirst ? particle : copied);
      CHECK(capture.m_pCap->instances.at(701).meshHash == meshId);
      CHECK(capture.m_pCap->instances.at(701).matHash == materialId);
      CHECK(capture.m_pCap->instances.at(702).meshHash == oldMeshName);
      CHECK(capture.m_pCap->instances.at(702).matHash == 977);
      CHECK(capture.m_pCap->meshes.size() == 2 && capture.m_pCap->materials.size() == 2);
      CHECK(capture.m_pCap->meshes.at(meshId)->matHash == materialId);
      CHECK(capture.m_pCap->meshes.at(oldMeshName)->matHash == 977);
      CHECK(blas.requestedContentKeys == std::vector<uint64_t>({977, 977}));
      CHECK(blas.input.geometryData.externalAssetIdentity.meshHash == (cachedAdmitted ? meshId : 0));
    }
  }
  return true;
}

static bool legacyBlasNamingIsUnchanged() {
  const auto ctx = std::make_shared<DxvkContext>();
  auto blas = makeBlas(1201, 1202, 1400, 1204, false);
  DrawCallState currentDraw = blas.input;
  currentDraw.materialData.hash = 1205;
  blas.contentMaterials.emplace(1205, currentDraw.materialData);
  auto instance = makeInstance(blas, 1206);
  instance.updateFrom(currentDraw);
  const auto oldBlasName = blas.input.getHash(RtxOptions::geometryAssetHashRule());
  CHECK(oldBlasName != 0);
  CHECK(oldBlasName != currentDraw.getHash(RtxOptions::geometryAssetHashRule()));
  CHECK(instance.getCaptureMeshAssetHash() == 0);
  CHECK(instance.getCaptureMaterialHash() == 1205);
  GameCapturer capture;
  capture.newInstance(ctx, instance);
  CHECK(capture.m_pCap->instances.at(1206).meshHash == oldBlasName);
  CHECK(capture.m_pCap->instances.at(1206).matHash == 1205);
  CHECK(capture.m_pCap->meshes.at(oldBlasName)->matHash == 1205);
  CHECK(blas.requestedContentKeys == std::vector<uint64_t> { 1205 });
  CHECK(capture.m_pCap->materials.at(1205).lssData.matName == hashToString(1205));
  return true;
}
} // namespace dxvk

int main() {
  try {
    if (!dxvk::sharedBlasAdmissionIsPerInstance() || !dxvk::legacyBlasNamingIsUnchanged() ||
        !dxvk::stableCaptureAndCopy() || !dxvk::fallbackNaming() ||
        !dxvk::legacyAndUnsupportedAdmission() || !dxvk::restartedCreationOrder()) return 1;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAILED exception: %s\n", error.what());
    return 1;
  }
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"rendererExecuted\":false,\"gpuExecuted\":false,\"scope\":\"extracted production capture functions with recording CPU collaborators\"}\n", checks);
  return 0;
}
