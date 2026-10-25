// Own CPU policy fixture for the applied production renderer header.
// No renderer, cache/BVH, capture, mod loader, game, SDK or GPU is executed.
#include "rtx_external_asset_identity.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>

namespace {
using Identity = dxvk::ExternalAssetIdentity;
static unsigned checks;
static constexpr std::uint64_t Mesh = 0xfedcba9876543210ull;
static constexpr std::uint64_t Material = 0x89abcdef01234567ull;
static constexpr std::uint64_t MeshFallback = 0x1020304050607080ull;
static constexpr std::uint64_t MaterialFallback = 0x8070605040302010ull;

static void Check(bool value, const char* message) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Pair(const Identity& identity, std::uint64_t mesh,
                 std::uint64_t material, const char* message) {
  Check(identity.meshHash == mesh && identity.materialHash == material, message);
}
static void Fallback(const Identity& identity, const char* message) {
  Check(identity.meshHashOr(MeshFallback) == MeshFallback &&
        identity.materialHashOr(MaterialFallback) == MaterialFallback, message);
}

static void MeshAdmission() {
  Pair(Identity{}, 0, 0, "default construction does not opt into external identity");
  Pair(Identity::ForMesh(true, 1, false, Mesh, Material), Mesh, Material,
       "explicit single rigid surface preserves all 64 identity bits");
  Pair(Identity::ForMesh(false, 1, false, Mesh, Material), 0, 0,
       "default-off preserves legacy renderer identity policy");
  for (std::uint32_t surfaces : {0u, 2u, 3u, (std::numeric_limits<std::uint32_t>::max)()}) {
    Pair(Identity::ForMesh(true, surfaces, false, Mesh, Material), 0, 0,
         "empty and multiple surfaces do not acquire one invented asset identity");
  }
  Pair(Identity::ForMesh(true, 1, true, Mesh, Material), 0, 0,
       "skin mesh creation stays outside the rigid external identity contract");
  Pair(Identity::ForMesh(true, 1, false, 0, Material), 0, 0,
       "missing mesh key rejects the complete pair");
  Pair(Identity::ForMesh(true, 1, false, Mesh, 0), 0, 0,
       "missing material key rejects the complete pair");
  Pair(Identity::ForMesh(true, 1, false, 0, 0), 0, 0,
       "missing keys cannot opt into stable identity");
  for (std::uint64_t mesh : {std::uint64_t(1), std::uint64_t(1) << 32,
       std::uint64_t(1) << 63, (std::numeric_limits<std::uint64_t>::max)()}) {
    Pair(Identity::ForMesh(true, 1, false, mesh, Material), mesh, Material,
         "mesh identity boundary values are not truncated or sign-extended");
  }
  for (std::uint64_t material : {std::uint64_t(1), std::uint64_t(1) << 32,
       std::uint64_t(1) << 63, (std::numeric_limits<std::uint64_t>::max)()}) {
    Pair(Identity::ForMesh(true, 1, false, Mesh, material), Mesh, material,
         "material identity boundary values are not truncated or sign-extended");
  }
  const auto original = Identity::ForMesh(true, 1, false, Mesh, Material);
  const auto changedMesh = Identity::ForMesh(true, 1, false, Mesh ^ (std::uint64_t(1) << 48), Material);
  const auto changedMaterial = Identity::ForMesh(true, 1, false, Mesh, Material ^ (std::uint64_t(1) << 56));
  Check(changedMesh.meshHash != original.meshHash && changedMesh.materialHash == original.materialHash,
        "changed supplied mesh identity changes only the mesh asset key");
  Check(changedMaterial.meshHash == original.meshHash && changedMaterial.materialHash != original.materialHash,
        "changed supplied material identity changes only the material asset key");
}

static void DrawAdmission() {
  const auto identity = Identity::ForMesh(true, 1, false, Mesh, Material);
  Pair(identity.ForDraw(true, false, false, false), Mesh, Material,
       "opaque rigid draw retains the admitted pair");
  for (unsigned mask = 0; mask < 16; ++mask) {
    const bool opaque = (mask & 1u) != 0;
    const bool skin = (mask & 2u) != 0;
    const bool particles = (mask & 4u) != 0;
    const bool gpuInstancing = (mask & 8u) != 0;
    const auto draw = identity.ForDraw(opaque, skin, particles, gpuInstancing);
    if (mask == 1u) {
      Pair(draw, Mesh, Material, "only the admitted opaque rigid draw keeps explicit keys");
    } else {
      Pair(draw, 0, 0, "non-Opaque, skin, particles and GPU instancing clear both keys");
      Fallback(draw, "an unsupported draw selects both legacy fallback identities");
    }
    Pair(identity, Mesh, Material, "draw qualification never mutates the stored mesh identity");
  }
  for (const Identity malformed : {Identity{}, Identity{Mesh, 0}, Identity{0, Material}}) {
    Pair(malformed.ForDraw(true, false, false, false), 0, 0,
         "even an ordinary draw rejects a missing half of the identity pair");
  }
}

static void CoupledFallback() {
  for (const Identity unavailable : {Identity{}, Identity{Mesh, 0}, Identity{0, Material}}) {
    Fallback(unavailable, "partial explicit identity never mixes explicit and legacy keys");
    Check(unavailable.meshHashOr(0) == 0 && unavailable.materialHashOr(0) == 0,
          "fallback preserves caller's absent legacy identity without inventing a key");
  }
  const Identity valid{Mesh, Material};
  Check(valid.meshHashOr(MeshFallback) == Mesh && valid.materialHashOr(MaterialFallback) == Material,
        "a complete pair selects both exact explicit identities");
  Check(valid.meshHashOr(0) == Mesh && valid.materialHashOr(0) == Material,
        "complete identities do not depend on legacy fallback availability");
  Pair(valid, Mesh, Material, "fallback accessors do not mutate the stored identity");
}

static void CreationOrderAndRestart() {
  const std::array<std::uint64_t, 4> meshes{{Mesh, Mesh + 1, Mesh ^ (std::uint64_t(1) << 40), 1}};
  const std::array<std::uint64_t, 4> materials{{Material, Material + 1, Material ^ (std::uint64_t(1) << 50), 1}};
  const std::array<std::size_t, 4> forward{{0, 1, 2, 3}};
  const std::array<std::size_t, 4> reverse{{3, 2, 1, 0}};
  const std::array<std::size_t, 4> shuffled{{2, 0, 3, 1}};
  // This own map exercises only the header's deterministic admission policy.
  // It is not the renderer's asset replacement table or cache implementation.
  std::map<std::uint64_t, std::uint64_t> expected;
  for (std::size_t index : forward) {
    const auto item = Identity::ForMesh(true, 1, false, meshes[index], materials[index]);
    expected.emplace(item.meshHash, item.materialHash);
  }
  for (const auto& order : {forward, reverse, shuffled}) {
    std::map<std::uint64_t, std::uint64_t> recreated;
    for (std::size_t index : order) {
      const auto item = Identity::ForMesh(true, 1, false, meshes[index], materials[index]);
      recreated.emplace(item.meshHash, item.materialHash);
      Pair(item, meshes[index], materials[index], "creation order does not introduce a process counter");
    }
    Check(recreated == expected, "fresh own CPU table has identical keys after changed creation order");
    recreated.clear();
    Check(recreated.empty(), "the test retires its own table before a fresh reconstruction");
    for (std::size_t index : reverse) {
      const auto item = Identity::ForMesh(true, 1, false, meshes[index], materials[index]);
      recreated.emplace(item.meshHash, item.materialHash);
    }
    Check(recreated == expected, "fresh reconstruction preserves supplied keys after retirement");
  }
}

static void AssetAndRuntimeStateAreSeparate() {
  struct OwnRecord {
    Identity asset;
    std::uint64_t geometryComponent;
    std::uint64_t instanceMotion;
    float worldTranslation;
  };
  const OwnRecord first{Identity::ForMesh(true, 1, false, Mesh, Material), 17, 23, 0};
  const OwnRecord second{Identity::ForMesh(true, 1, false, Mesh, Material), 91, 47, 125};
  Check(first.geometryComponent != second.geometryComponent && first.instanceMotion != second.instanceMotion &&
        first.worldTranslation != second.worldTranslation, "own CPU records carry distinct runtime component and motion state");
  Check(first.asset.meshHash == second.asset.meshHash && first.asset.materialHash == second.asset.materialHash,
        "asset admission is independent of the fixture's separate component and motion state");
  const std::map<std::uint64_t, unsigned> ownReplacementChoices{{Mesh, 7}};
  Check(ownReplacementChoices.at(first.asset.meshHash) == ownReplacementChoices.at(second.asset.meshHash),
        "representative own CPU lookup addresses the same explicit asset across runtime-state changes");
  const auto changedContentKey = Identity::ForMesh(true, 1, false, Mesh + 1, Material);
  Check(ownReplacementChoices.find(changedContentKey.meshHash) == ownReplacementChoices.end(),
        "a changed supplied content key cannot select the fixture's older asset entry");
  Check(first.geometryComponent == 17 && second.geometryComponent == 91 && first.instanceMotion == 23 && second.instanceMotion == 47,
        "the header leaves independent component and instance/motion values untouched");
}
}

int main() {
  MeshAdmission(); DrawAdmission(); CoupledFallback();
  CreationOrderAndRestart(); AssetAndRuntimeStateAreSeparate();
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"rendererExecuted\":false,\"scope\":\"applied production header CPU admission policy only\"}\n", checks);
}
