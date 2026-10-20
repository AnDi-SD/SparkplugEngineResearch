// Own CPU fixture for values crossing the conversion/API boundary. No game,
// native object, D3D/COM, Remix API, or renderer freshness is qualified here.
#include "winx_rigid_geometry_packet.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

using Builder = winx_remix::RigidGeometryPacketBuilder;
static unsigned checks;
static void Check(bool value, const char* name) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static bool Empty(const Builder::Packet& value) {
  return !value.valid && value.vertices.empty() && value.provenance.vertexCoverage.empty() &&
    value.provenance.indexCoverage.empty() && !value.provenance.scene && !value.provenance.scope &&
    !value.material.recipe.valid && !value.material.texture.valid && !value.material.recipe.material;
}
static Builder::Provenance Source() {
  Builder::Provenance value;
  value.scene = 11; value.camera = 12; value.renderer = 13; value.support = 14; value.object = 15;
  value.model = 16; value.mesh = 17; value.frame = 18; value.modelOrdinal = 19;
  value.modelOccurrences = 2; value.registrations = 3;
  value.scope = 21; value.mutation = 22; value.updateSequence = 23; value.deviceEpoch = 24;
  value.cpuVertexGeneration = 31; value.cpuIndexGeneration = 32;
  value.vertexGeneration = 41; value.indexGeneration = 42; value.declarationGeneration = 43;
  value.textureGeneration = 51; value.textureContentGeneration = 52;
  value.originallySelected = true; value.computedWorld = true;
  value.cpuVertexPartial = true; value.cpuIndexPartial = false;
  value.vertexCoverage = {{12, 36}, {48, 84}}; value.indexCoverage = {{2, 8}};
  value.world[0] = value.world[5] = value.world[10] = value.world[15] = 1.0f;
  value.world[12] = 1.25f;
  return value;
}
static Builder::Material Material() {
  Builder::Material value;
  auto& recipe = value.recipe;
  recipe.valid = true; recipe.renderer = 13; recipe.material = 61; recipe.pass = 62; recipe.layer = 63;
  recipe.textureOwner = 64; recipe.texture = 65; recipe.textureCOM = 66; recipe.effectiveOverride = 1;
  for (unsigned i = 0; i < recipe.rawTexture.size(); ++i) recipe.rawTexture[i] = 100 + i;
  for (unsigned i = 0; i < recipe.rawMaterial.size(); ++i) recipe.rawMaterial[i] = 200 + i;
  for (unsigned i = 0; i < recipe.stage.size(); ++i) recipe.stage[i] = 300 + i;
  for (unsigned i = 0; i < recipe.sampler.size(); ++i) recipe.sampler[i] = 400 + i;
  for (unsigned i = 0; i < recipe.mappedStates.size(); ++i) {
    recipe.mappedStates[i] = 500 + i; recipe.mappedPresent[i] = std::uint8_t(i % 2);
  }
  value.texture.valid = true; value.texture.device = 67; value.texture.texture = 66;
  value.texture.generation = 51; value.texture.contentGeneration = 52;
  value.texture.width = 4; value.texture.height = 2; value.texture.levels = 3; value.texture.format = 21;
  value.equation.rgb = {3, 1, 2}; value.equation.alpha = {4, 2, 1};
  value.equation.factor = 0x12345678; value.equation.coordinates = 0; value.equation.transformFlags = 0;
  for (unsigned i = 0; i < 16; ++i) value.equation.transform[i] = float(i) / 16;
  value.instance.cull = 1; value.instance.test = 1; value.instance.function = 7; value.instance.reference = 127;
  value.instance.writeMask = 15; value.instance.blendEnabled = 1; value.instance.source = 5;
  value.instance.destination = 6; value.instance.operation = 1; value.instance.channels = true;
  value.bootstrap.textureSrgb = 1; value.bootstrap.separateAlpha = 0; value.bootstrap.stencil = 0;
  return value;
}
struct Conversion {
  std::vector<Builder::Vertex> vertices = std::vector<Builder::Vertex>(3);
  Builder::Plan plan;
  Builder::Material material = Material();
  mutable unsigned calls = 0;
  bool success = true;
  Conversion() {
    vertices[0].position[0] = 1.0f; vertices[1].position[1] = 2.0f; vertices[2].position[2] = 3.0f;
    for (auto& vertex : vertices) { vertex.normal[2] = 1.0f; vertex.texcoord[0] = .25f; vertex.color = 0x12345678; }
    plan.albedo[0] = 1.0f; plan.albedo[1] = .5f; plan.emission[2] = .125f;
    plan.vertexRGB = true; plan.vertexAlpha = true; plan.alpha = 127;
  }
  bool operator()(std::vector<Builder::Vertex>& output, Builder::Plan& outputPlan, Builder::Provenance&,
                  Builder::Material& outputMaterial) const {
    ++calls; output = vertices; outputPlan = plan; outputMaterial = material; return success;
  }
};

int main() {
  const auto current = [] { return true; };
  auto source = Source(); Conversion conversion;
  unsigned fences = 0;
  auto packet = Builder::Capture(source, conversion, [&] { ++fences; return true; });
  Check(packet.valid && packet.vertices.size() == 3, "ordinary owned triangle accepted");
  Check(conversion.calls == 1 && fences == 2, "one conversion between current fences");
  Check(packet.provenance.scene == 11 && packet.provenance.camera == 12 && packet.provenance.renderer == 13,
    "scene boundary numeric provenance copied");
  Check(packet.provenance.support == 14 && packet.provenance.object == 15 && packet.provenance.model == 16 &&
    packet.provenance.mesh == 17, "object numeric provenance copied");
  Check(packet.provenance.frame == 18 && packet.provenance.modelOrdinal == 19 &&
    packet.provenance.modelOccurrences == 2 && packet.provenance.registrations == 3,
    "model ordinal occurrences and support registrations remain distinct");
  Check(packet.provenance.scope == 21 && packet.provenance.mutation == 22 &&
    packet.provenance.updateSequence == 23 && packet.provenance.deviceEpoch == 24, "boundary epochs copied");
  Check(packet.provenance.cpuVertexGeneration == 31 && packet.provenance.cpuIndexGeneration == 32 &&
    packet.provenance.vertexGeneration == 41 && packet.provenance.indexGeneration == 42 &&
    packet.provenance.declarationGeneration == 43 && packet.provenance.textureGeneration == 51 &&
    packet.provenance.textureContentGeneration == 52, "independent provenance generations are not conflated");
  Check(packet.provenance.originallySelected && packet.provenance.computedWorld &&
    packet.provenance.cpuVertexPartial && !packet.provenance.cpuIndexPartial, "qualification provenance flags copied");
  Check(packet.provenance.vertexCoverage == source.vertexCoverage && packet.provenance.indexCoverage == source.indexCoverage,
    "partial interval provenance copied without inferring coverage");
  Check(packet.plan.albedo[0] == 1 && packet.plan.albedo[1] == .5f && packet.plan.emission[2] == .125f &&
    packet.plan.vertexRGB && packet.plan.vertexAlpha && packet.plan.alpha == 127, "material channel plan values copied");
  const auto& recipe = packet.material.recipe;
  Check(recipe.valid && recipe.renderer == 13 && recipe.material == 61 && recipe.pass == 62 && recipe.layer == 63 &&
    recipe.textureOwner == 64 && recipe.texture == 65 && recipe.textureCOM == 66 && recipe.effectiveOverride == 1,
    "raw material recipe identities and override copied independently of geometry identity");
  Check(recipe.rawTexture == conversion.material.recipe.rawTexture && recipe.rawMaterial == conversion.material.recipe.rawMaterial &&
    recipe.stage == conversion.material.recipe.stage && recipe.sampler == conversion.material.recipe.sampler &&
    recipe.mappedStates == conversion.material.recipe.mappedStates && recipe.mappedPresent == conversion.material.recipe.mappedPresent,
    "raw texture material mapped state presence and sampler arrays own every source element");
  const auto& texture = packet.material.texture;
  Check(texture.valid && texture.device == 67 && texture.texture == 66 && texture.generation == 51 &&
    texture.contentGeneration == 52 && texture.width == 4 && texture.height == 2 && texture.levels == 3 && texture.format == 21,
    "texture transport identity layout lifecycle and content generations copied without owning texels");
  const auto& equation = packet.material.equation;
  Check(equation.rgb.operation == 3 && equation.rgb.first == 1 && equation.rgb.second == 2 &&
    equation.alpha.operation == 4 && equation.alpha.first == 2 && equation.alpha.second == 1 &&
    equation.factor == 0x12345678 && equation.coordinates == 0 && equation.transformFlags == 0 && equation.transform[15] == 15.0f / 16,
    "material equation and bootstrap supplied transform stay separate from raw recipe");
  const auto& instance = packet.material.instance;
  Check(instance.cull == 1 && instance.test == 1 && instance.function == 7 && instance.reference == 127 &&
    instance.writeMask == 15 && instance.blendEnabled == 1 && instance.source == 5 && instance.destination == 6 &&
    instance.operation == 1 && instance.channels && packet.material.bootstrap.textureSrgb == 1 &&
    !packet.material.bootstrap.separateAlpha && !packet.material.bootstrap.stencil,
    "instance and device bootstrap values are owned separately from original material states");
  source.world[12] = 99; source.vertexCoverage[0] = {100, 200}; source.indexCoverage.clear();
  conversion.vertices[0].position[0] = 99; conversion.vertices.clear(); conversion.plan.albedo[0] = 0;
  conversion.material = {};
  Check(packet.provenance.world[12] == 1.25f && packet.provenance.vertexCoverage[0].first == 12 &&
    packet.provenance.indexCoverage.size() == 1, "provenance owns world and intervals after source mutation");
  Check(packet.vertices[0].position[0] == 1 && packet.vertices[0].color == 0x12345678 && packet.plan.albedo[0] == 1,
    "packet owns geometry and plan after converter source retirement");
  Check(packet.material.recipe.rawTexture[8] == 108 && packet.material.recipe.rawMaterial[10] == 210 &&
    packet.material.recipe.stage[28] == 328 && packet.material.recipe.sampler[13] == 413 &&
    packet.material.recipe.mappedStates[255] == 755 && packet.material.recipe.mappedPresent[255] == 1 &&
    packet.material.texture.generation == 51 && packet.material.texture.contentGeneration == 52 &&
    packet.material.equation.factor == 0x12345678 && packet.material.instance.reference == 127 &&
    packet.material.bootstrap.textureSrgb == 1, "material recipe equation texture and bootstrap survive converter source retirement");

  source = Source(); conversion = Conversion{};
  fences = 0;
  bool convertedAfterInitialFence = false;
  const auto emitted = Builder::Capture(source,
    [&](std::vector<Builder::Vertex>& vertices, Builder::Plan& plan, Builder::Provenance& owned, Builder::Material& material) {
      convertedAfterInitialFence = fences == 1;
      if (!conversion(vertices, plan, owned, material)) return false;
      owned.cpuVertexGeneration = 301; owned.cpuIndexGeneration = 302;
      owned.vertexGeneration = 401; owned.indexGeneration = 402; owned.declarationGeneration = 403;
      owned.textureGeneration = 501; owned.textureContentGeneration = 502;
      material.texture.generation = 501; material.texture.contentGeneration = 502;
      owned.cpuVertexPartial = false; owned.cpuIndexPartial = true;
      owned.vertexCoverage = {{24, 48}}; owned.indexCoverage = {{6, 12}, {18, 24}};
      owned.world[12] = 2.5f;
      return true;
    }, [&] { ++fences; return true; });
  Check(emitted.valid && convertedAfterInitialFence && fences == 2,
    "converter acquires actual resource provenance after initial current fence");
  Check(emitted.provenance.cpuVertexGeneration == 301 && emitted.provenance.cpuIndexGeneration == 302 &&
    emitted.provenance.vertexGeneration == 401 && emitted.provenance.indexGeneration == 402 &&
    emitted.provenance.declarationGeneration == 403 && emitted.provenance.textureGeneration == 501 &&
    emitted.provenance.textureContentGeneration == 502, "converter emitted resource generations retained independently");
  Check(!emitted.provenance.cpuVertexPartial && emitted.provenance.cpuIndexPartial &&
    emitted.provenance.vertexCoverage == std::vector<std::pair<std::size_t, std::size_t>>{{24, 48}} &&
    emitted.provenance.indexCoverage == std::vector<std::pair<std::size_t, std::size_t>>{{6, 12}, {18, 24}} &&
    emitted.provenance.world[12] == 2.5f, "converter emitted intervals flags and world retained as owned values");
  Check(source.cpuVertexGeneration == 31 && source.world[12] == 1.25f && source.vertexCoverage[0].first == 12,
    "converter resource provenance writes do not mutate initial context");
  conversion = Conversion{};
  Check(Empty(Builder::Capture(source, conversion, [] { return false; })) && conversion.calls == 0,
    "failed initial fence does not convert or expose partial packet");
  fences = 0;
  Check(Empty(Builder::Capture(source, conversion, [&] { return ++fences == 1; })) && conversion.calls == 1 && fences == 2,
    "failed final fence discards copied geometry and provenance");
  conversion = Conversion{}; conversion.success = false;
  Check(Empty(Builder::Capture(source, conversion, current)), "converter failure discards its populated output");
  conversion = Conversion{}; conversion.vertices.clear();
  Check(Empty(Builder::Capture(source, conversion, current)), "empty conversion rejected");
  conversion.vertices.resize(4);
  Check(Empty(Builder::Capture(source, conversion, current)), "incomplete triangle rejected");
  conversion.vertices.resize(Builder::kVertexLimit);
  Check(Builder::Capture(source, conversion, current).valid, "exact triangle capacity accepted");
  conversion.vertices.resize(Builder::kVertexLimit + 3);
  Check(Empty(Builder::Capture(source, conversion, current)), "triangle above capacity rejected");

  conversion = Conversion{};
  for (unsigned field = 0; field < 13; ++field) {
    auto bad = conversion;
    switch (field) {
    case 0: bad.material.recipe.valid = false; break;
    case 1: bad.material.texture.valid = false; break;
    case 2: bad.material.texture.device = 0; break;
    case 3: bad.material.texture.texture = 0; break;
    case 4: bad.material.texture.generation = 0; break;
    case 5: bad.material.texture.contentGeneration = 0; break;
    case 6: bad.material.texture.width = 0; break;
    case 7: bad.material.texture.height = 0; break;
    case 8: bad.material.texture.levels = 0; break;
    case 9: ++bad.material.recipe.renderer; break;
    case 10: ++bad.material.recipe.textureCOM; break;
    case 11: ++bad.material.texture.generation; break;
    case 12: ++bad.material.texture.contentGeneration; break;
    }
    unsigned finalFences = 0;
    Check(Empty(Builder::Capture(source, bad, [&] { ++finalFences; return true; })) && finalFences == 1,
      "invalid material or inconsistent texture identity layout and generation rejects complete packet before final fence");
  }
  const float invalid[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity()};
  for (float value : invalid) {
    for (unsigned group = 0; group < 3; ++group) {
      auto bad = conversion;
      float* fields = group == 0 ? bad.vertices[1].position : group == 1 ? bad.vertices[1].normal : bad.vertices[1].texcoord;
      const unsigned count = group == 2 ? 2 : 3;
      for (unsigned field = 0; field < count; ++field) {
        const float old = fields[field]; fields[field] = value;
        Check(Empty(Builder::Capture(source, bad, current)), "nonfinite vertex component rejected");
        fields[field] = old;
      }
    }
    for (unsigned field = 0; field < 16; ++field) {
      auto bad = source; bad.world[field] = value;
      Check(Empty(Builder::Capture(bad, conversion, current)), "nonfinite world component rejected");
      auto material = conversion; material.material.equation.transform[field] = value;
      Check(Empty(Builder::Capture(source, material, current)), "nonfinite owned material transform component rejected");
    }
    Check(Empty(Builder::Capture(source,
      [&](std::vector<Builder::Vertex>& vertices, Builder::Plan& plan, Builder::Provenance& owned, Builder::Material& material) {
        const bool result = conversion(vertices, plan, owned, material); owned.world[7] = value; return result;
      }, current)), "converter emitted nonfinite world rejected after conversion");
  }
  const float invalidCoefficients[] = {-1.0f, 1.0001f, std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
  for (float value : invalidCoefficients) for (unsigned group = 0; group < 2; ++group) for (unsigned field = 0; field < 3; ++field) {
    auto bad = conversion; (group ? bad.plan.emission : bad.plan.albedo)[field] = value;
    Check(Empty(Builder::Capture(source, bad, current)), "out of bounded material coefficient rejected");
  }
  auto endpoints = conversion;
  for (unsigned i = 0; i < 3; ++i) { endpoints.plan.albedo[i] = 0; endpoints.plan.emission[i] = 1; }
  Check(Builder::Capture(source, endpoints, current).valid, "material coefficient endpoints accepted");
  auto partial = source;
  partial.vertexCoverage.resize(Builder::kCoverageLimit, {0, 1});
  partial.indexCoverage.resize(Builder::kCoverageLimit, {4, 7});
  Check(Builder::Capture(partial, conversion, current).valid, "exact per-buffer interval metadata capacity accepted");
  partial.vertexCoverage.emplace_back(9, 12);
  Check(Empty(Builder::Capture(partial, conversion, current)), "excess vertex coverage metadata rejected");
  partial = source; partial.indexCoverage.resize(Builder::kCoverageLimit + 1, {0, 1});
  Check(Empty(Builder::Capture(partial, conversion, current)), "excess index coverage metadata rejected");
  Check(Builder::Capture(source,
    [&](std::vector<Builder::Vertex>& vertices, Builder::Plan& plan, Builder::Provenance& owned, Builder::Material& material) {
      const bool result = conversion(vertices, plan, owned, material);
      owned.vertexCoverage.resize(Builder::kCoverageLimit, {0, 1});
      owned.indexCoverage.resize(Builder::kCoverageLimit, {2, 3});
      return result;
    }, current).valid, "exact converter emitted interval capacity accepted");
  for (unsigned group = 0; group < 2; ++group) {
    unsigned finalFences = 0;
    Check(Empty(Builder::Capture(source,
      [&](std::vector<Builder::Vertex>& vertices, Builder::Plan& plan, Builder::Provenance& owned, Builder::Material& material) {
        const bool result = conversion(vertices, plan, owned, material);
        (group ? owned.indexCoverage : owned.vertexCoverage).resize(Builder::kCoverageLimit + 1, {0, 1});
        return result;
      }, [&] { ++finalFences; return true; })) && finalFences == 1,
      "converter emitted interval overflow rejected before final fence");
  }

  bool converterException = false;
  try {
    Builder::Capture(source, [](std::vector<Builder::Vertex>& vertices, Builder::Plan&, Builder::Provenance&, Builder::Material&) -> bool {
      vertices.resize(3); throw std::bad_alloc();
    }, current);
  } catch (const std::bad_alloc&) { converterException = true; }
  Check(converterException, "converter allocation exception propagates");
  Check(Builder::Capture(source, conversion, current).valid, "failed allocation leaves no cached packet or poison state");
  bool fenceException = false;
  try {
    Builder::Capture(source, conversion, []() -> bool { throw std::runtime_error("fixture current"); });
  } catch (const std::runtime_error&) { fenceException = true; }
  Check(fenceException, "current exception propagates");
  Check(Builder::Capture(source, conversion, current).valid, "later capture remains independent after fence exception");
  Check(packet.valid && packet.vertices.size() == 3 && packet.provenance.world[12] == 1.25f,
    "independent later captures preserve earlier owned packet");
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"scope\":\"owned rigid geometry conversion packet only\"}\n", checks);
  return 0;
}
