#pragma once
// Own passive diagnostic adapter, included after the complete surface/scene
// adapters. It never submits, invokes a native producer, calls a COM method,
// marks Bytes verified, or retains a borrowed resource pointer. A completed
// record describes one bounded observation, not lifetime, FreshRt, material,
// visibility, or draw eligibility.
#include <array>
#include <sstream>
#include <string>

namespace geometry_snapshot_observer {
namespace abi = sparkplug::evidence::pc;
static bool enabled;
static FILE* output;
static DWORD ownerThread;
static unsigned sampledScopes, completedScopes, rejectedScopes, rows, worldQualified, meshQualified;
static constexpr size_t logByteLimit = 16 * 1024 * 1024, scopeByteLimit = 4 * 1024 * 1024;
static constexpr unsigned rowLimit = 4096;
static uint64_t nextObservation, revision;
static bool observing;

static bool WriteLine(const char* bytes, size_t count) {
  if (!output) return false;
  const auto position = _ftelli64(output);
  if (position < 0 || uint64_t(position) > logByteLimit || count > logByteLimit - size_t(position)) return false;
  return fwrite(bytes, 1, count, output) == count && fflush(output) == 0;
}
static void Rejected(uint64_t scope, const char* reason) {
  ++rejectedScopes;
  char line[512]{};
  const int count = snprintf(line, sizeof(line),
    "{\"event\":\"scope_rejected\",\"frame\":%u,\"draw\":%u,\"scope\":%llu,\"reason\":\"%s\",\"completed\":false,\"fencesStable\":false}\n",
    frameId, drawId, scope, reason);
  if (count > 0 && size_t(count) < sizeof(line)) WriteLine(line, size_t(count));
}

#if defined(_M_IX86)
#if defined(WINX_REMIX_TEST)
// Only owned CPU fixtures substitute phase qualification, never production
// inputs. No native fixed-address mapping is necessary in those fixtures.
static bool (*getWitness)(uint32_t, uint32_t, uint64_t, native_update_source::Witness&) = native_update_source::GetWitness;
static bool (*qualifyWitness)(const native_update_source::Witness&, uint32_t, uint32_t, uint64_t) = native_update_source::QualifyWitness;
#endif
static bool GetWitness(uint32_t scene, uint32_t frame, uint64_t mutation, native_update_source::Witness& witness) {
#if defined(WINX_REMIX_TEST)
  return getWitness && getWitness(scene, frame, mutation, witness);
#else
  return native_update_source::GetWitness(scene, frame, mutation, witness);
#endif
}
static bool QualifyWitness(const native_update_source::Witness& witness, uint32_t scene, uint32_t frame, uint64_t mutation) {
#if defined(WINX_REMIX_TEST)
  return qualifyWitness && qualifyWitness(witness, scene, frame, mutation);
#else
  return native_update_source::QualifyWitness(witness, scene, frame, mutation);
#endif
}
template<class T> static bool Read(uintptr_t address, T& value) {
  return scene_geometry::Read(address, &value, sizeof(value));
}
static bool Prerequisites() {
  const auto thread = GetCurrentThreadId();
  return sceneGeometryObservationEnabled && native_owner_source::enabled && native_mesh_source::enabled &&
    native_transport_source::enabled && native_update_source::enabled && native_camera_source::enabled &&
    thread == ownerThread && thread == scene_geometry::renderThread && thread == native_owner_source::ownerThread &&
    thread == native_mesh_source::ownerThread && thread == native_update_source::ownerThread &&
    thread == native_camera_source::ownerThread;
}
static bool Camera(uint32_t address, abi::spCameraObservedLayout& camera) {
  if (!address || !Read(address, camera) || !native_camera_source::KnownCamera(camera.base.base.base.vtableAddress) ||
      camera.projectionBranch || camera.twoDimensional || camera.projectionMatrix[11] != 1.0f ||
      camera.projectionMatrix[15] != 0.0f) return false;
  for (float value : camera.viewMatrix) if (!std::isfinite(value)) return false;
  for (float value : camera.projectionMatrix) if (!std::isfinite(value)) return false;
  return true;
}
static bool Engine(uint32_t scene, uint32_t camera) {
  uint32_t engine = 0, currentScene = 0, currentCamera = 0;
  return Read(native_owner_source::enginePointerAddress, engine) && engine &&
    Read(engine + 0x18, currentScene) && currentScene == scene &&
    Read(engine + 0x1c, currentCamera) && currentCamera == camera;
}
struct Context {
  uint32_t scene = 0, camera = 0, manager = 0, frame = 0, draw = 0, system = 0, root = 0;
  uint64_t scope = 0, epoch = 0, mutation = 0, revision = 0;
};
static bool ScopeCurrent(const scene_geometry::Scope& scope, const Context& context) {
  uint32_t system = 0, root = 0;
  return enabled && Prerequisites() && observing && revision == context.revision &&
    scene_geometry::active == &scope && !scope.parent && !scope.manager &&
    scope.serial == context.scope && scope.scene == context.scene && scope.camera == context.camera &&
    frameId == context.frame && drawId == context.draw && native_camera_source::deviceEpoch == context.epoch &&
    scene_geometry::MutationSerial() == context.mutation && Engine(context.scene, context.camera) &&
    Read(context.scene + 0x38, system) && system == context.system && system &&
    Read(system + 0x1d4, root) && root == context.root &&
    scene_geometry::MutationSerial() == context.mutation && native_camera_source::deviceEpoch == context.epoch &&
    frameId == context.frame && drawId == context.draw && revision == context.revision;
}
static void Words(std::ostringstream& text, const char* name, const uint32_t* words, unsigned count) {
  text << ",\"" << name << "\":[";
  for (unsigned i = 0; i < count; ++i) { if (i) text << ','; text << words[i]; }
  text << ']';
}
template<size_t Count>
static void FloatWords(std::ostringstream& text, const char* name, const float (&values)[Count]) {
  uint32_t words[Count]{};
  memcpy(words, values, sizeof(words));
  Words(text, name, words, unsigned(Count));
}
static const char* Boolean(bool value) { return value ? "true" : "false"; }

struct Row {
  uint32_t support = 0, object = 0, supportTable = 0, ownerPrimary = 0, registrations = 0;
  bool originallySelected = false, supportRead = false, objectRead = false;
  uint32_t worldSphereBits[4]{}, objectFlags = 0, matrixDirty = 0, worldBits[16]{};
  bool worldQualified = false, computedWorld = false;
  unsigned worldReason = independent_scene_source::Count;
  uint32_t model = 0, modelOrdinal = 0, modelOccurrences = 0, mesh = 0, material = 0;
  bool renderablesValid = false, renderablesRead = false, modelRead = false, modelAbi = false, callbacksEmpty = false;
  bool meshHeaders = false, declarationRead = false, transportWitness = false, resourcesResolved = false;
  bool vertexUploadComplete = false, indexUploadComplete = false, cpuVertexMatched = false, cpuIndexMatched = false;
  bool cpuVertexPartial = false, cpuIndexPartial = false, transportCurrent = false, meshQualified = false;
  uint32_t nativeVertexCount = 0, nativePrimitiveCount = 0, stride = 0, componentFlags = 0;
  uint32_t vertexBuffer = 0, indexBuffer = 0, declaration = 0, nativeDeclaration = 0;
  uint32_t vertexBytes = 0, indexBytes = 0, indexType = 0, vertexBegin = 0, indexBegin = 0;
  uint64_t cpuVertexGeneration = 0, cpuIndexGeneration = 0;
  uint64_t vertexGeneration = 0, indexGeneration = 0, declarationGeneration = 0;
};
static void Mesh(const std::unique_lock<std::recursive_mutex>& borrow, IDirect3DDevice9* device,
                 independent_scene_source::Input& input, Row& row) {
  if (!row.worldQualified || !row.modelAbi || !row.callbacksEmpty || !device) return;
  abi::spDXMeshObservedLayout mesh{};
  native_mesh_source::ResourcePair pair{};
  uint32_t declaration[7]{};
  if (!Read(row.mesh, mesh)) return;
  row.nativeVertexCount = mesh.base.base.vertexCount; row.nativePrimitiveCount = mesh.base.base.primitiveCount;
  row.stride = mesh.vertexStride; row.componentFlags = mesh.base.base.vertexComponentFlags;
  row.indexType = mesh.indexType; row.vertexBegin = mesh.vertexBegin; row.indexBegin = mesh.indexBegin;
  row.nativeDeclaration = mesh.vertexDeclaration;
  row.meshHeaders = native_mesh_source::ReadResourceHeaders(mesh, pair);
  row.declarationRead = Read(mesh.vertexDeclaration, declaration);
  if (!row.meshHeaders || !row.declarationRead) return;
  row.vertexBuffer = pair.vb.direct3DVertexBuffer; row.indexBuffer = pair.ib.direct3DIndexBuffer;
  row.declaration = declaration[6]; row.vertexBytes = pair.vb.byteSize; row.indexBytes = pair.ib.byteSize;
  native_mesh_source::TransportWitness transport{};
  row.transportWitness = native_transport_source::Witness(borrow, device,
    reinterpret_cast<void*>(row.vertexBuffer), reinterpret_cast<void*>(row.indexBuffer),
    reinterpret_cast<void*>(row.declaration), transport);
  if (!row.transportWitness) return;
  row.vertexGeneration = transport.vertexGeneration; row.indexGeneration = transport.indexGeneration;
  row.declarationGeneration = transport.declarationGeneration;
  native_mesh_source::Geometry geometry{};
  row.resourcesResolved = native_mesh_source::ResolveCurrentResources(borrow, device, row.mesh,
    input.owner.renderer, input.owner.world, transport, geometry);
  if (row.resourcesResolved) {
    row.cpuVertexGeneration = geometry.vertices->generation; row.cpuIndexGeneration = geometry.indices->generation;
    row.cpuVertexPartial = geometry.vertices->partial; row.cpuIndexPartial = geometry.indices->partial;
    const auto vertices = surfaceBuffers.find(geometry.vertexBuffer), indices = surfaceBuffers.find(geometry.indexBuffer);
    row.vertexUploadComplete = vertices != surfaceBuffers.end() && vertices->second.complete;
    row.indexUploadComplete = indices != surfaceBuffers.end() && indices->second.complete;
    // Always compare each available upload independently. Never set verified,
    // even when both bytes agree; partial captures retain their explicit flag.
    if (row.vertexUploadComplete) row.cpuVertexMatched = native_mesh_source::EqualsUpload(*geometry.vertices, vertices->second.bytes);
    if (row.indexUploadComplete) row.cpuIndexMatched = native_mesh_source::EqualsUpload(*geometry.indices, indices->second.bytes);
  }
  row.transportCurrent = native_transport_source::Current(borrow, transport);
  row.meshQualified = row.resourcesResolved && row.vertexUploadComplete && row.indexUploadComplete &&
    row.cpuVertexMatched && row.cpuIndexMatched && row.transportCurrent;
  // All borrowed pointers die here while borrow still holds guard. Row owns
  // only numeric identities, generation values, counts and comparison results.
}
static std::string Describe(const Row& row) {
  std::ostringstream text;
  text << "{\"support\":" << row.support << ",\"object\":" << row.object << ",\"supportTable\":" << row.supportTable
    << ",\"ownerPrimary\":" << row.ownerPrimary << ",\"registrations\":" << row.registrations
    << ",\"originallySelected\":" << Boolean(row.originallySelected) << ",\"supportRead\":" << Boolean(row.supportRead)
    << ",\"objectRead\":" << Boolean(row.objectRead) << ",\"objectFlags\":" << row.objectFlags << ",\"matrixDirty\":" << row.matrixDirty
    << ",\"worldQualified\":" << Boolean(row.worldQualified) << ",\"worldReason\":" << row.worldReason
    << ",\"computedWorld\":" << Boolean(row.computedWorld) << ",\"model\":" << row.model
    << ",\"modelOrdinal\":" << row.modelOrdinal << ",\"modelOccurrences\":" << row.modelOccurrences
    << ",\"mesh\":" << row.mesh << ",\"material\":" << row.material
    << ",\"renderablesValid\":" << Boolean(row.renderablesValid) << ",\"renderablesRead\":" << Boolean(row.renderablesRead)
    << ",\"modelRead\":" << Boolean(row.modelRead)
    << ",\"modelAbi\":" << Boolean(row.modelAbi) << ",\"callbacksEmpty\":" << Boolean(row.callbacksEmpty)
    << ",\"meshHeaders\":" << Boolean(row.meshHeaders) << ",\"declarationRead\":" << Boolean(row.declarationRead)
    << ",\"transportWitness\":" << Boolean(row.transportWitness) << ",\"resourcesResolved\":" << Boolean(row.resourcesResolved)
    << ",\"vertexUploadComplete\":" << Boolean(row.vertexUploadComplete) << ",\"indexUploadComplete\":" << Boolean(row.indexUploadComplete)
    << ",\"cpuVertexMatched\":" << Boolean(row.cpuVertexMatched) << ",\"cpuIndexMatched\":" << Boolean(row.cpuIndexMatched)
    << ",\"cpuVertexPartial\":" << Boolean(row.cpuVertexPartial) << ",\"cpuIndexPartial\":" << Boolean(row.cpuIndexPartial)
    << ",\"transportCurrent\":" << Boolean(row.transportCurrent) << ",\"meshQualified\":" << Boolean(row.meshQualified)
    << ",\"nativeVertexCount\":" << row.nativeVertexCount << ",\"nativePrimitiveCount\":" << row.nativePrimitiveCount
    << ",\"stride\":" << row.stride << ",\"componentFlags\":" << row.componentFlags
    << ",\"vertexBuffer\":" << row.vertexBuffer << ",\"indexBuffer\":" << row.indexBuffer
    << ",\"declaration\":" << row.declaration << ",\"nativeDeclaration\":" << row.nativeDeclaration
    << ",\"vertexBytes\":" << row.vertexBytes << ",\"indexBytes\":" << row.indexBytes << ",\"indexType\":" << row.indexType
    << ",\"vertexBegin\":" << row.vertexBegin << ",\"indexBegin\":" << row.indexBegin
    << ",\"cpuVertexGeneration\":" << row.cpuVertexGeneration << ",\"cpuIndexGeneration\":" << row.cpuIndexGeneration
    << ",\"vertexGeneration\":" << row.vertexGeneration << ",\"indexGeneration\":" << row.indexGeneration
    << ",\"declarationGeneration\":" << row.declarationGeneration;
  Words(text, "worldSphereBits", row.worldSphereBits, 4);
  Words(text, "worldBits", row.worldBits, 16);
  text << '}';
  return text.str();
}

static bool Fences(scene_geometry::Scope& scope, const Context& context,
                   const native_update_source::Witness& witness, const uint32_t (&managerHeader)[15],
                   const std::vector<uint32_t>& selected, const abi::spCameraObservedLayout& cameraInput) {
  uint32_t finalManager[15]{};
  abi::spCameraObservedLayout finalCamera{};
  std::vector<uint32_t> finalSelected;
  // Qualifier reads precede the final camera/selection copies. There is no
  // callback/native producer between those copies and the final context fence.
  return ScopeCurrent(scope, context) && QualifyWitness(witness, context.scene, context.frame, context.mutation) &&
    Read(context.manager, finalManager) && !memcmp(managerHeader, finalManager, sizeof(managerHeader)) &&
    scene_geometry::Vector(context.manager, 0x28, finalSelected) && finalSelected == selected &&
    Camera(context.camera, finalCamera) && !memcmp(&cameraInput, &finalCamera, sizeof(cameraInput)) && ScopeCurrent(scope, context);
}

static void ObserveOwned(scene_geometry::Scope& scope, uint32_t manager, uint32_t scene, uint32_t camera) {
  std::unique_lock<std::recursive_mutex> borrow(guard);
  ++sampledScopes;
  if (observing) { ++revision; Rejected(scope.serial, "reentry"); return; }
  observing = true;
  struct Reset { ~Reset() { observing = false; } } reset;
  Context context{scene, camera, manager, frameId, drawId, 0, 0, scope.serial,
    native_camera_source::deviceEpoch, scene_geometry::MutationSerial(), ++revision};
  if (!Prerequisites() || scope.parent || scene_geometry::active != &scope || scope.scene != scene ||
      scope.camera != camera || scope.manager || native_owner_source::activeModel || native_owner_source::activeSupport ||
      !manager || !Engine(scene, camera)) { Rejected(scope.serial, "scope"); return; }
  uint32_t managerHeader[15]{};
  abi::spCameraObservedLayout cameraInput{};
  if (!Read(manager, managerHeader) || managerHeader[0] != 0x6e8cdc || managerHeader[0x38 / 4] ||
      !Camera(camera, cameraInput)) { Rejected(scope.serial, "selection_or_camera"); return; }
  // Manager+40 is a plane-stack header, not a scene field. Scene identity is
  // joined only through callback/Scope, the current engine and root witness.
  std::vector<uint32_t> selected;
  if (!scene_geometry::Vector(manager, 0x28, selected)) { Rejected(scope.serial, "selection_vector"); return; }
  const auto& registry = scope.RegistrySnapshot();
  if (!registry.valid || !registry.occurrencesComplete || registry.scene != scene ||
      registry.mutationSerial != context.mutation) { Rejected(scope.serial, "registry"); return; }
  context.system = registry.system; context.root = registry.root;
  native_update_source::Witness witness{};
  if (!GetWitness(scene, context.frame, context.mutation, witness) || !witness.valid ||
      witness.scene != scene || witness.frame != context.frame || witness.mutation != context.mutation ||
      witness.deviceEpoch != context.epoch || witness.thread != ownerThread ||
      witness.system != context.system || witness.root != context.root || !ScopeCurrent(scope, context)) {
    Rejected(scope.serial, "update_witness"); return;
  }
  const std::set<uint32_t> selectedSet(selected.begin(), selected.end());
  std::map<uint32_t, unsigned> registrations;
  for (const auto& occurrence : registry.occurrences) ++registrations[occurrence.support];
  uint32_t renderer = 0, deviceAddress = 0;
  if (!Read(native_owner_source::rendererPointerAddress, renderer) ||
      scene_geometry::Word(renderer) != abi::spPCRendererPrimaryVTable || !Read(renderer + 0xc9e8, deviceAddress)) {
    renderer = deviceAddress = 0;
  }
  auto* device = reinterpret_cast<IDirect3DDevice9*>(deviceAddress);
  std::ostringstream prefix;
  prefix << "{\"event\":\"scope\",\"schema\":1,\"observation\":" << ++nextObservation
    << ",\"frame\":" << context.frame << ",\"draw\":" << context.draw << ",\"scene\":" << scene << ",\"camera\":" << camera
    << ",\"scope\":" << context.scope << ",\"epoch\":" << context.epoch << ",\"mutation\":" << context.mutation
    << ",\"selectedSlots\":" << selected.size() << ",\"supports\":" << registry.supports.size()
    << ",\"occurrences\":" << registry.occurrences.size() << ",\"updateValid\":true,\"updateSequence\":" << witness.sequence;
  FloatWords(prefix, "viewBits", cameraInput.viewMatrix); FloatWords(prefix, "projectionBits", cameraInput.projectionMatrix);
  prefix << ",\"rows\":[";
  std::string packet = prefix.str();
  unsigned rowCount = 0, worldCount = 0, meshCount = 0;
  bool complete = true;
  const char* reason = "complete";
  const auto append = [&](const Row& row) {
    if (rowCount >= rowLimit) return false;
    const auto text = Describe(row);
    if (text.size() + packet.size() + 512 > scopeByteLimit) return false;
    if (rowCount) packet.push_back(',');
    packet.append(text); ++rowCount;
    if (row.worldQualified) ++worldCount;
    if (row.meshQualified) ++meshCount;
    return true;
  };
  for (uint32_t supportAddress : registry.supports) {
    Row base{}; base.support = supportAddress; base.registrations = registrations[supportAddress];
    base.originallySelected = selectedSet.count(supportAddress) != 0;
    abi::spRenderSupportObservedLayout support{};
    base.supportRead = Read(supportAddress, support);
    if (!base.supportRead) {
      base.worldReason = independent_scene_source::Support;
      if (!append(base)) { complete = false; reason = "row_or_byte_limit"; break; }
      continue;
    }
    base.object = support.completeObject; base.supportTable = support.vtable;
    memcpy(base.worldSphereBits, support.worldSphere, sizeof(base.worldSphereBits));
    independent_scene_source::Input input{};
    input.witness = witness; input.originallySelected = base.originallySelected;
    auto& owner = input.owner;
    owner.scene = scene; owner.system = context.system; owner.root = context.root; owner.camera = camera;
    owner.support = supportAddress; owner.object = support.completeObject; owner.renderer = renderer;
    owner.frame = context.frame; owner.sceneScope = context.scope; owner.mutation = context.mutation;
    owner.registrations = base.registrations; base.objectRead = Read(owner.object, owner.ownerPrimary);
    base.ownerPrimary = owner.ownerPrimary;
    owner.ownerKind = support.vtable == abi::spRenderNodeSupportVTable ? 2 : support.vtable == 0x6f4528 ? 1 : 0;
    if (support.vtable == abi::spRenderNodeSupportVTable) {
      abi::spRenderNodeLayout node{}; base.objectRead = Read(owner.object, node);
      if (base.objectRead) { base.objectFlags = node.prefix.base.flags; base.matrixDirty = node.matrixDirty; }
    }
    if (!owner.ownerPrimary) base.worldReason = independent_scene_source::Support;
    base.worldQualified = owner.ownerPrimary && independent_scene_source::ReadWorldInput(input, support,
      [&](independent_scene_source::Reason rejected) { base.worldReason = rejected; return false; });
    base.computedWorld = input.computedWorld;
    if (base.worldQualified) memcpy(base.worldBits, &owner.world, sizeof(base.worldBits));
    base.renderablesValid = support.renderableBegin <= support.renderableEnd && support.renderableEnd <= support.renderableCapacity &&
      !((support.renderableEnd - support.renderableBegin) % 4) && support.renderableEnd - support.renderableBegin <= 4096 * 4;
    if (!base.renderablesValid) {
      if (!append(base)) { complete = false; reason = "row_or_byte_limit"; break; }
      continue;
    }
    std::map<uint32_t, std::pair<unsigned, unsigned>> models;
    bool readable = true;
    for (unsigned ordinal = 0; ordinal < (support.renderableEnd - support.renderableBegin) / 4; ++ordinal) {
      uint32_t model = 0;
      if (!Read(support.renderableBegin + ordinal * 4, model)) { readable = false; break; }
      if (model) { auto& entry = models[model]; if (!entry.first) entry.second = ordinal; ++entry.first; }
    }
    base.renderablesRead = readable;
    if (!readable || models.empty()) {
      if (!append(base)) { complete = false; reason = "row_or_byte_limit"; break; }
      continue;
    }
    for (const auto& item : models) {
      Row row = base; row.model = item.first; row.modelOccurrences = item.second.first; row.modelOrdinal = item.second.second;
      abi::spModelLayout model{}; row.modelRead = Read(row.model, model);
      row.modelAbi = row.modelRead && model.base.base.base.vtableAddress == 0x6eaa58 && model.baseMeshData;
      if (row.modelRead) { row.mesh = model.baseMeshData; row.material = model.base.material; }
      row.callbacksEmpty = row.modelAbi && !model.base.callback2C && !model.base.callback30 &&
        independent_scene_source::EmptyCallbacks(model.base.callbacks34) && independent_scene_source::EmptyCallbacks(model.base.callbacks44);
      input.owner.model = row.model; input.owner.mesh = row.mesh;
      Mesh(borrow, device, input, row);
      if (!append(row)) { complete = false; reason = "row_or_byte_limit"; break; }
    }
    if (!complete) break;
  }
  const bool stable = Fences(scope, context, witness, managerHeader, selected, cameraInput);
  if (!stable) { complete = false; reason = "stale_fences"; }
  std::ostringstream suffix;
  suffix << "],\"candidateRows\":" << rowCount << ",\"candidateWorldQualified\":" << worldCount
    << ",\"candidateMeshQualified\":" << meshCount << ",\"completed\":" << Boolean(complete)
    << ",\"fencesStable\":" << Boolean(stable) << ",\"reason\":\"" << reason
    << "\",\"freshRtQualified\":false,\"lifetimeQualified\":false,\"materialQualified\":false,\"drawEligibilityQualified\":false}\n";
  packet.append(suffix.str());
  // No native read/API/callback is made between this full final fence and write.
  if (!Fences(scope, context, witness, managerHeader, selected, cameraInput)) { Rejected(scope.serial, "final_fence"); return; }
  if (packet.size() > scopeByteLimit || !WriteLine(packet.data(), packet.size())) { Rejected(scope.serial, "log_capacity_or_write"); return; }
  if (complete) { ++completedScopes; rows += rowCount; worldQualified += worldCount; meshQualified += meshCount; }
  else ++rejectedScopes;
}
static void Observe(scene_geometry::Scope& scope, uint32_t manager, uint32_t scene, uint32_t camera) noexcept {
  if (!enabled || frameId % 300 || GetCurrentThreadId() != ownerThread) return;
  try { ObserveOwned(scope, manager, scene, camera); }
  catch (const std::bad_alloc&) { Rejected(scope.serial, "allocation"); }
  catch (...) { Rejected(scope.serial, "exception"); }
}
#endif

static void Initialize() {
  wchar_t path[MAX_PATH]{};
  const auto length = GetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_SNAPSHOT_OBSERVER", path, MAX_PATH);
  if (!length) return; // Disabled path installs no callback and performs no reads.
  enabled = false;
  if (length >= MAX_PATH) { sceneGeometryObservationEnabled = false; return; }
  output = _wfsopen(path, L"wb", _SH_DENYNO);
  ownerThread = GetCurrentThreadId();
  const char* reason = "architecture";
#if defined(_M_IX86)
  enabled = output && Prerequisites();
  reason = !output ? "file" : enabled ? "ready" : "prerequisites";
  if (enabled) scene_geometry::observeOriginalSelection = Observe;
#endif
  if (!enabled) sceneGeometryObservationEnabled = false;
  char line[768]{};
  const int count = snprintf(line, sizeof(line),
    "{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"reason\":\"%s\",\"thread\":%lu,\"maxRows\":4096,\"samplePeriod\":300,\"maxScopeBytes\":4194304,\"maxLogBytes\":16777216,\"passive\":true,\"nativeProducersCalled\":false,\"gpuCalls\":false,\"freshRtQualified\":false,\"lifetimeQualified\":false,\"materialQualified\":false,\"drawEligibilityQualified\":false}\n",
    enabled ? "true" : "false", reason, ownerThread);
  if (count <= 0 || size_t(count) >= sizeof(line) || !WriteLine(line, size_t(count))) {
    enabled = false; sceneGeometryObservationEnabled = false;
#if defined(_M_IX86)
    if (scene_geometry::observeOriginalSelection == Observe) scene_geometry::observeOriginalSelection = nullptr;
#endif
  }
}
} // namespace geometry_snapshot_observer
