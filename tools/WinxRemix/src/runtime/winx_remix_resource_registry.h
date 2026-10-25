#pragma once
// Own bounded Remix API resource ownership, not reconstructed game logic.
// Keys and API hashes are supplied by the caller. This registry does not infer
// asset identity, observe native pointers, or own the API library/device.
#include <remix/remix_c.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <new>
#include <vector>

namespace winx_remix {
struct RemixResourceRegistryTestAccess;

class RemixResourceRegistry final {
public:
  struct MeshEntry {
    remixapi_MeshHandle handle; unsigned frame; std::size_t bytes; remixapi_MaterialHandle material;
    unsigned failedPressureFrame = 0; bool pressureFailed = false;
    bool usable = true; // False while pending or quarantined after a failed Create.
    unsigned influences = 0; // Zero for immutable baked/unskinned geometry.
    unsigned requiredBones = 0; // One past the largest index in the immutable mesh.
  };
  struct MaterialEntry {
    remixapi_MaterialHandle handle; unsigned frame;
    unsigned failedPressureFrame = 0; bool pressureFailed = false;
    bool usable = true; // False while pending or quarantined after a failed Create.
  };
  struct ResourceStats {
    std::size_t meshBytes = 0;
    unsigned meshCreates = 0, meshDestroys = 0, materialCreates = 0, materialDestroys = 0;
    unsigned resourceFailures = 0;
    unsigned pressureRequests = 0, pressureMeshDestroys = 0, pressureMaterialDestroys = 0, pressureRejected = 0;
    unsigned reentryRejected = 0;
  };
  static constexpr std::size_t MeshLimit = 1024;
  static constexpr std::size_t MaterialLimit = 256;
  static constexpr std::size_t MeshByteLimit = 64 * 1024 * 1024;

  // The caller's recursive guard also protects surrounding draw qualification.
  // Reject nested resource operations while retaining retirement requests for
  // one drain at the outer boundary. API reentry may invalidate Stable().
  class Operation final {
  public:
    explicit Operation(RemixResourceRegistry& registry)
      : registry_(registry), lock_(registry.mutex_), retireSerial_(registry.retireSerial_), frame_(registry.frame_) {
      if (registry_.busy_) { ++registry_.stats_.reentryRejected; return; }
      if (registry_.retirePending_ && !registry_.retireDraining_) {
        const bool all = registry_.retireAll_;
        registry_.retirePending_ = registry_.retireAll_ = false;
        registry_.retireDraining_ = true;
        registry_.Retire(all);
        registry_.retireDraining_ = false;
        if (registry_.retirePending_) { ++registry_.stats_.reentryRejected; return; }
      }
      retireSerial_ = registry_.retireSerial_;
      frame_ = registry_.frame_;
      registry_.busy_ = owned = true;
    }
    Operation(const Operation&) = delete;
    Operation& operator=(const Operation&) = delete;
    Operation(Operation&&) = delete;
    Operation& operator=(Operation&&) = delete;

    bool Stable() const {
      return owned && retireSerial_ == registry_.retireSerial_ && frame_ == registry_.frame_;
    }
    ~Operation() {
      if (!owned) return;
      registry_.busy_ = false;
      if (registry_.retirePending_ && !registry_.retireDraining_) {
        const bool all = registry_.retireAll_;
        registry_.retirePending_ = registry_.retireAll_ = false;
        registry_.retireDraining_ = true;
        registry_.Retire(all);
        registry_.retireDraining_ = false;
      }
    }
    bool owned = false;

  private:
    friend class RemixResourceRegistry;
    RemixResourceRegistry& registry_;
    std::unique_lock<std::recursive_mutex> lock_;
    std::uint64_t retireSerial_;
    unsigned frame_;
  };

  RemixResourceRegistry(std::recursive_mutex& mutex, const unsigned& frame, remixapi_Interface* (*api)())
    : mutex_(mutex), frame_(frame), api_(api) {}
  RemixResourceRegistry(const RemixResourceRegistry&) = delete;
  RemixResourceRegistry& operator=(const RemixResourceRegistry&) = delete;
  RemixResourceRegistry(RemixResourceRegistry&&) = delete;
  RemixResourceRegistry& operator=(RemixResourceRegistry&&) = delete;

  std::uint64_t Epoch() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return ownershipEpoch_;
  }
  bool MeshCurrent(remixapi_MeshHandle handle, remixapi_MaterialHandle material = nullptr) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (busy_ || retirePending_ || !handle) return false;
    for (const auto& item : meshes_) if (item.second.handle == handle && item.second.usable &&
        (!material || item.second.material == material)) {
      for (const auto& entry : materials_)
        if (entry.second.handle == item.second.material && entry.second.usable) return true;
      return false;
    }
    return false;
  }

  void Retire(bool all = false) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!all && frame_ % 30) return;
    ++ownershipEpoch_; ++retireSerial_;
    if (busy_) { retirePending_ = true; retireAll_ |= all; return; }
    if (meshes_.empty() && materials_.empty()) return;
    Operation operation(*this);
    if (!operation.owned) return;
    auto api = Api();
    if (!api || !api->DestroyMesh || !api->DestroyMaterial) return;
    std::uint64_t meshKeys[MeshLimit]{}, materialKeys[MaterialLimit]{};
    std::size_t meshes = 0, materials = 0;
    for (auto& item : meshes_) {
      if (all) item.second.frame = frame_ - 301u;
      if (frame_ - item.second.frame > 300 && meshes < MeshLimit) meshKeys[meshes++] = item.first;
    }
    for (std::size_t i = 0; i < meshes; ++i) DestroyMesh(meshKeys[i]);
    for (auto& item : materials_) {
      if (all) item.second.frame = frame_ - 301u;
      if (frame_ - item.second.frame > 300 && materials < MaterialLimit) materialKeys[materials++] = item.first;
    }
    for (std::size_t i = 0; i < materials; ++i) DestroyMaterial(materialKeys[i]);
  }

  bool EnsureRoom(std::size_t meshes, std::size_t bytes, std::size_t materials,
                  remixapi_MaterialHandle protectedMaterial = nullptr) {
    Operation operation(*this);
    if (!operation.owned) return false;
    return EnsureRoomOwned(meshes, bytes, materials, protectedMaterial) && operation.Stable();
  }
  // Run inside an owned Operation. Pressure never retires current-frame
  // resources. Scratch is bounded and uses no heap allocation.
  bool EnsureRoomOwned(std::size_t meshes, std::size_t bytes, std::size_t materials,
                       remixapi_MaterialHandle protectedMaterial = nullptr) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!busy_) return false;
    const auto room = [&]() {
      return meshes <= MeshLimit && materials <= MaterialLimit && bytes <= MeshByteLimit &&
        meshes_.size() <= MeshLimit - meshes && materials_.size() <= MaterialLimit - materials &&
        stats_.meshBytes <= MeshByteLimit - bytes;
    };
    if (room()) return true;
    ++stats_.pressureRequests;
    if (meshes > MeshLimit || materials > MaterialLimit || bytes > MeshByteLimit) {
      ++stats_.pressureRejected; return false;
    }
    auto api = Api();
    if (!api || !api->DestroyMesh || !api->DestroyMaterial) { ++stats_.pressureRejected; return false; }
    const auto orphans = [&]() {
      std::uint64_t keys[MaterialLimit]{}; std::size_t count = 0;
      for (const auto& item : materials_) {
        const auto& value = item.second;
        if (value.frame != frame_ && value.handle != protectedMaterial &&
            !(value.pressureFailed && value.failedPressureFrame == frame_) &&
            !MaterialReferenced(value.handle) && count < MaterialLimit) keys[count++] = item.first;
      }
      for (std::size_t i = 0; i < count; ++i) { DestroyMaterial(keys[i], true); if (room()) break; }
    };
    orphans(); if (room()) return true;
    struct Candidate { std::uint64_t key; unsigned age; };
    Candidate candidates[MeshLimit]{}; std::size_t count = 0;
    for (const auto& item : meshes_) {
      const auto& value = item.second;
      if (value.frame != frame_ && !(value.pressureFailed && value.failedPressureFrame == frame_) && count < MeshLimit)
        candidates[count++] = { item.first, frame_ - value.frame };
    }
    std::sort(candidates, candidates + count, [](const Candidate& a, const Candidate& b) {
      return a.age != b.age ? a.age > b.age : a.key < b.key;
    });
    for (std::size_t index = 0; index < count && !room(); ++index) {
      remixapi_MaterialHandle referenced = nullptr;
      {
        const auto it = meshes_.find(candidates[index].key);
        if (it == meshes_.end()) continue;
        referenced = it->second.material;
      }
      if (meshes_.size() <= MeshLimit - meshes && stats_.meshBytes <= MeshByteLimit - bytes) {
        bool eligible = false;
        for (const auto& entry : materials_) if (entry.second.handle == referenced) {
          const auto& material = entry.second;
          eligible = material.frame != frame_ && material.handle != protectedMaterial &&
            !(material.pressureFailed && material.failedPressureFrame == frame_);
          break;
        }
        if (eligible) for (const auto& mesh : meshes_)
          if (mesh.second.material == referenced && mesh.second.frame == frame_) { eligible = false; break; }
        if (!eligible) continue;
      }
      if (DestroyMesh(candidates[index].key, true)) orphans();
    }
    const bool result = room(); if (!result) ++stats_.pressureRejected;
    return result;
  }

  // True means the key is occupied, including quarantined entries; callers
  // must not try to create a second resource when the returned handle is null.
  bool FindMaterialOwned(std::uint64_t key, const Operation& operation, remixapi_MaterialHandle& output) {
    output = nullptr;
    if (!Owns(operation)) return false;
    const auto it = materials_.find(key);
    if (it == materials_.end()) return false;
    if (it->second.usable && operation.Stable()) { it->second.frame = frame_; output = it->second.handle; }
    return true;
  }
  // Recover the caller-supplied material identity from the current live owner.
  // A numeric API handle alone is not a content identity or lifetime token.
  bool MaterialIdentityOwned(remixapi_MaterialHandle handle, const Operation& operation,
                             std::uint64_t& output) const {
    output = 0;
    if (!Owns(operation) || !operation.Stable() || !handle) return false;
    for (const auto& item : materials_) if (item.second.handle == handle && item.second.usable) {
      output = item.first;
      return output != 0;
    }
    return false;
  }
  bool FindMeshOwned(std::uint64_t key, const Operation& operation, remixapi_MeshHandle& output,
                     remixapi_MaterialHandle expectedMaterial = nullptr) {
    output = nullptr;
    if (!Owns(operation)) return false;
    const auto it = meshes_.find(key);
    if (it == meshes_.end()) return false;
    if (it->second.usable && operation.Stable() &&
        (!expectedMaterial || it->second.material == expectedMaterial)) {
      it->second.frame = frame_; output = it->second.handle;
    }
    return true;
  }

  // Allocate the ownership node before Create. Even an unsuccessful Create
  // that writes a handle remains owned until Destroy succeeds.
  remixapi_MaterialHandle CreateMaterialOwned(const remixapi_MaterialInfo& info, const Operation& operation) {
    if (!Owns(operation) || !operation.Stable()) return nullptr;
    {
      auto it = materials_.find(info.hash);
      if (it != materials_.end()) {
        if (!it->second.usable) return nullptr;
        it->second.frame = frame_; return it->second.handle;
      }
    }
    auto api = Api();
    if (!api || !api->CreateMaterial || !api->DestroyMaterial ||
        !EnsureRoomOwned(0, 0, 1) || !operation.Stable()) return nullptr;
    try {
      if (!materials_.emplace(info.hash, MaterialEntry{ nullptr, frame_, 0, false, false }).second) return nullptr;
    } catch (const std::bad_alloc&) { ++stats_.resourceFailures; return nullptr; }
    ++ownershipEpoch_;
    remixapi_MaterialHandle handle = nullptr;
    remixapi_ErrorCode result = REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    try { result = api->CreateMaterial(&info, &handle); } catch (const std::bad_alloc&) {}
    {
      auto it = materials_.find(info.hash);
      if (!handle) { materials_.erase(it); ++ownershipEpoch_; ++stats_.resourceFailures; return nullptr; }
      ++stats_.materialCreates; it->second.handle = handle;
      it->second.usable = result == REMIXAPI_ERROR_CODE_SUCCESS && operation.Stable();
      if (it->second.usable) return handle;
      it->second.frame = frame_ - 301u;
    }
    ++stats_.resourceFailures; DestroyMaterial(info.hash); return nullptr;
  }

  remixapi_MeshHandle CreateGeometryOwned(const std::vector<remixapi_HardcodedVertex>& vertices,
      const std::vector<std::uint32_t>& indices, remixapi_MaterialHandle material, std::uint64_t key,
      const Operation& operation, const remixapi_MeshInfoSkinning* skinning = nullptr) {
    if (!Owns(operation)) return nullptr;
    const auto api = Api();
    if (!operation.Stable() || !api || !api->CreateMesh || !api->DestroyMesh ||
        !material || vertices.empty() || indices.empty()) return nullptr;
    bool liveMaterial = false;
    for (const auto& item : materials_) liveMaterial |= item.second.handle == material && item.second.usable;
    if (!liveMaterial) return nullptr;
    {
      auto found = meshes_.find(key);
      if (found != meshes_.end()) {
        if (!found->second.usable || found->second.material != material ||
            found->second.influences != (skinning ? skinning->bonesPerVertex : 0) || !operation.Stable()) return nullptr;
        found->second.frame = frame_; return found->second.handle;
      }
    }
    if (vertices.size() > MeshByteLimit / sizeof(vertices[0]) || indices.size() > MeshByteLimit / 4 || indices.size() % 3)
      return nullptr;
    std::size_t bytes = vertices.size() * sizeof(vertices[0]) + indices.size() * 4;
    unsigned requiredBones = 0;
    if (skinning) {
      const auto b = skinning->bonesPerVertex;
      if (!b || b > 8 || !skinning->blendWeights_values || !skinning->blendIndices_values ||
          skinning->blendWeights_count != vertices.size() * b || skinning->blendIndices_count != vertices.size() * b)
        return nullptr;
      for (std::uint32_t i = 0; i < skinning->blendWeights_count; ++i) {
        if (!std::isfinite(skinning->blendWeights_values[i]) || skinning->blendWeights_values[i] < 0 ||
            skinning->blendIndices_values[i] >= 256) return nullptr;
        requiredBones = (std::max)(requiredBones, skinning->blendIndices_values[i] + 1);
      }
      bytes += std::size_t(skinning->blendWeights_count) * 4 + std::size_t(skinning->blendIndices_count) * 4;
    }
    if (bytes > MeshByteLimit) return nullptr;
    for (const auto index : indices) if (index >= vertices.size()) return nullptr;
    remixapi_MeshInfoSurfaceTriangles surface{};
    surface.vertices_values = vertices.data(); surface.vertices_count = vertices.size();
    surface.indices_values = indices.data(); surface.indices_count = indices.size(); surface.material = material;
    if (skinning) { surface.skinning_hasvalue = 1; surface.skinning_value = *skinning; }
    remixapi_MeshInfo info{}; info.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO; info.hash = key;
    info.surfaces_values = &surface; info.surfaces_count = 1;
    if (!EnsureRoomOwned(1, bytes, 0, material) || !operation.Stable()) return nullptr;
    try {
      if (!meshes_.emplace(key, MeshEntry{ nullptr, frame_, bytes, material, 0, false, false,
          skinning ? skinning->bonesPerVertex : 0, requiredBones }).second) return nullptr;
    } catch (const std::bad_alloc&) { ++stats_.resourceFailures; return nullptr; }
    stats_.meshBytes += bytes; ++ownershipEpoch_;
    remixapi_MeshHandle handle = nullptr;
    remixapi_ErrorCode result = REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    try { result = api->CreateMesh(&info, &handle); } catch (const std::bad_alloc&) {}
    {
      auto it = meshes_.find(key);
      if (!handle) {
        stats_.meshBytes -= it->second.bytes; meshes_.erase(it); ++ownershipEpoch_; ++stats_.resourceFailures; return nullptr;
      }
      ++stats_.meshCreates; it->second.handle = handle;
      it->second.usable = result == REMIXAPI_ERROR_CODE_SUCCESS && operation.Stable();
      if (it->second.usable) return handle;
      it->second.frame = frame_ - 301u;
    }
    ++stats_.resourceFailures; DestroyMesh(key); return nullptr;
  }

  // Match the Skin draw's qualification/touch order. The caller separately
  // validates the influence count and matrix values before this ownership step.
  bool TouchForDraw(remixapi_MeshHandle mesh, remixapi_MaterialHandle material,
                    unsigned influences, std::size_t boneCount) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!busy_) return false;
    bool foundMesh = false, foundMaterial = false;
    for (auto& item : meshes_) if (item.second.handle == mesh && item.second.material == material &&
        item.second.usable && item.second.influences == influences) {
      if (influences && boneCount < item.second.requiredBones) return false;
      item.second.frame = frame_; foundMesh = true; break;
    }
    for (auto& item : materials_) if (item.second.handle == material && item.second.usable) {
      item.second.frame = frame_; foundMaterial = true; break;
    }
    return foundMesh && foundMaterial;
  }

  void RecordFailure() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ++stats_.resourceFailures;
  }
  // Read-only views have the same surrounding-guard requirement as the old
  // cache's diagnostics. They expose neither mutable storage nor API ownership.
  const std::map<std::uint64_t, MeshEntry>& Meshes() const noexcept { return meshes_; }
  const std::map<std::uint64_t, MaterialEntry>& Materials() const noexcept { return materials_; }
  const ResourceStats& Stats() const noexcept { return stats_; }
  bool Busy() const { std::lock_guard<std::recursive_mutex> lock(mutex_); return busy_; }
  bool RetirementPending() const { std::lock_guard<std::recursive_mutex> lock(mutex_); return retirePending_; }
  bool RetirementDraining() const { std::lock_guard<std::recursive_mutex> lock(mutex_); return retireDraining_; }

private:
  friend struct RemixResourceRegistryTestAccess;
  bool Owns(const Operation& operation) const {
    return &operation.registry_ == this && operation.owned && operation.lock_.owns_lock();
  }
  remixapi_Interface* Api() const { return api_ ? api_() : nullptr; }
  bool MaterialReferenced(remixapi_MaterialHandle handle) const {
    for (const auto& item : meshes_) if (item.second.material == handle) return true;
    return false;
  }
  // No node iterator/reference crosses an API call, including failures.
  bool DestroyMesh(std::uint64_t key, bool pressure = false) {
    MeshEntry saved{};
    {
      const auto it = meshes_.find(key);
      if (it == meshes_.end() || !it->second.handle) return false;
      saved = it->second;
    }
    auto api = Api(); remixapi_ErrorCode result = REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    try { if (api && api->DestroyMesh) result = api->DestroyMesh(saved.handle); } catch (const std::bad_alloc&) {}
    auto it = meshes_.find(key);
    if (it == meshes_.end() || it->second.handle != saved.handle) return false;
    if (result != REMIXAPI_ERROR_CODE_SUCCESS) {
      ++stats_.resourceFailures; it->second.pressureFailed = true; it->second.failedPressureFrame = frame_; return false;
    }
    ++stats_.meshDestroys; if (pressure) ++stats_.pressureMeshDestroys;
    stats_.meshBytes -= it->second.bytes; meshes_.erase(it); ++ownershipEpoch_; return true;
  }
  bool DestroyMaterial(std::uint64_t key, bool pressure = false) {
    MaterialEntry saved{};
    {
      const auto it = materials_.find(key);
      if (it == materials_.end() || !it->second.handle || MaterialReferenced(it->second.handle)) return false;
      saved = it->second;
    }
    auto api = Api(); remixapi_ErrorCode result = REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    try { if (api && api->DestroyMaterial) result = api->DestroyMaterial(saved.handle); } catch (const std::bad_alloc&) {}
    auto it = materials_.find(key);
    if (it == materials_.end() || it->second.handle != saved.handle) return false;
    if (result != REMIXAPI_ERROR_CODE_SUCCESS) {
      ++stats_.resourceFailures; it->second.pressureFailed = true; it->second.failedPressureFrame = frame_; return false;
    }
    ++stats_.materialDestroys; if (pressure) ++stats_.pressureMaterialDestroys;
    materials_.erase(it); ++ownershipEpoch_; return true;
  }

  std::recursive_mutex& mutex_;
  const unsigned& frame_;
  remixapi_Interface* (*api_)();
  std::map<std::uint64_t, MeshEntry> meshes_;
  std::map<std::uint64_t, MaterialEntry> materials_;
  ResourceStats stats_{};
  bool busy_ = false, retirePending_ = false, retireAll_ = false, retireDraining_ = false;
  std::uint64_t ownershipEpoch_ = 1, retireSerial_ = 0;
};
} // namespace winx_remix
