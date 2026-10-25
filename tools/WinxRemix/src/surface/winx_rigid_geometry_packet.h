#pragma once
// Own geometry values at the converter/API boundary.
// Numeric provenance is copied diagnostic input, not a native lifetime token,
// persistent asset identity, FreshRt proof, or permission to submit a draw.
// The adapter owns native qualification and must recheck it at consumption.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "../materials/winx_material_recipe_source.h"

namespace winx_remix {
class RigidGeometryPacketBuilder final {
public:
  static constexpr std::size_t kVertexLimit = 32768 * 3;
  static constexpr std::size_t kCoverageLimit = 4096;

  struct Vertex {
    float position[3]{}, normal[3]{}, texcoord[2]{};
    std::uint32_t color = 0;
  };
  struct Plan {
    float albedo[3]{}, emission[3]{};
    bool vertexRGB = false, vertexAlpha = false;
    std::uint8_t alpha = 255;
  };
  struct Provenance {
    std::uint32_t scene = 0, camera = 0, renderer = 0, support = 0, object = 0;
    std::uint32_t model = 0, mesh = 0, frame = 0, modelOrdinal = 0;
    std::uint32_t modelOccurrences = 0, registrations = 0;
    std::uint64_t scope = 0, mutation = 0, updateSequence = 0, deviceEpoch = 0;
    std::uint64_t cpuVertexGeneration = 0, cpuIndexGeneration = 0;
    std::uint64_t vertexGeneration = 0, indexGeneration = 0, declarationGeneration = 0;
    std::uint64_t textureGeneration = 0, textureContentGeneration = 0;
    bool originallySelected = false, computedWorld = false;
    bool cpuVertexPartial = false, cpuIndexPartial = false;
    float world[16]{};
    // Copied half-open intervals in the source buffer's coordinates. Capture
    // neither interprets their coverage nor compares them to uploaded bytes.
    std::vector<std::pair<std::size_t, std::size_t>> vertexCoverage, indexCoverage;
  };
  struct TextureProvenance {
    bool valid = false;
    // Numeric operation-local identities only. The adapter must reacquire and
    // qualify its transport witness; these values neither own texels nor name
    // a persistent Toolkit asset.
    std::uintptr_t device = 0, texture = 0;
    std::uint64_t generation = 0, contentGeneration = 0;
    std::uint32_t width = 0, height = 0, levels = 0, format = 0;
  };
  struct Channel { std::uint32_t operation = 0, first = 0, second = 0; };
  struct Equation {
    Channel rgb, alpha;
    std::uint32_t factor = 0, coordinates = 0, transformFlags = 0;
    float transform[16]{};
  };
  struct Instance {
    std::uint32_t cull = 0, test = 0, function = 0, reference = 0, writeMask = 0;
    std::uint32_t blendEnabled = 0, source = 0, destination = 0, operation = 0;
    bool channels = false;
  };
  struct Bootstrap {
    std::uint32_t textureSrgb = 0, separateAlpha = 0, stencil = 0;
  };
  struct Material {
    MaterialRecipeSource::Recipe recipe;
    TextureProvenance texture;
    // Bootstrap and the adapter's equation/instance projection are separate
    // from the native recipe and the own material coefficients in Plan.
    Equation equation;
    Instance instance;
    Bootstrap bootstrap;
  };
  struct Packet {
    std::vector<Vertex> vertices;
    Plan plan;
    Provenance provenance;
    Material material;
    bool valid = false;
  };

  // convert(vector<Vertex>&, Plan&, Provenance&, Material&) acquires and consumes fresh
  // guarded native inputs only after the initial current fence, performs the
  // existing pure geometry conversion and fills its owned resource provenance.
  // current() is supplied by the native adapter;
  // passing it twice does not extend that adapter's qualification lifetime.
  // No cached packet or borrowed input survives. Allocation/converter/current
  // exceptions propagate, as in the existing conversion caller.
  template<class Converter, class Current>
  static Packet Capture(const Provenance& provenance, const Converter& convert,
                        const Current& current) {
    if (!current() || !Finite(provenance.world) ||
        provenance.vertexCoverage.size() > kCoverageLimit ||
        provenance.indexCoverage.size() > kCoverageLimit) return {};
    Packet output;
    output.provenance = provenance;
    if (!convert(output.vertices, output.plan, output.provenance, output.material) || output.vertices.empty() ||
        output.vertices.size() > kVertexLimit || output.vertices.size() % 3 ||
        !Bounded(output.plan.albedo) || !Bounded(output.plan.emission) ||
        !Finite(output.provenance.world) || !Finite(output.material.equation.transform) ||
        !MaterialConsistent(output.material, output.provenance) ||
        output.provenance.vertexCoverage.size() > kCoverageLimit ||
        output.provenance.indexCoverage.size() > kCoverageLimit) return {};
    for (const auto& vertex : output.vertices)
      if (!Finite(vertex.position) || !Finite(vertex.normal) || !Finite(vertex.texcoord)) return {};
    // All source copies and converter work precede this final current fence.
    if (!current()) return {};
    output.valid = true;
    return output;
  }

private:
  static bool MaterialConsistent(const Material& material, const Provenance& provenance) {
    const auto& texture = material.texture;
    return material.recipe.valid && texture.valid && texture.device && texture.texture &&
      texture.generation && texture.contentGeneration && texture.width && texture.height && texture.levels &&
      material.recipe.renderer == provenance.renderer &&
      material.recipe.textureCOM == texture.texture &&
      texture.generation == provenance.textureGeneration &&
      texture.contentGeneration == provenance.textureContentGeneration;
  }
  template<std::size_t Count>
  static bool Finite(const float (&values)[Count]) {
    for (float value : values) if (!std::isfinite(value)) return false;
    return true;
  }
  static bool Bounded(const float (&values)[3]) {
    for (float value : values)
      if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
    return true;
  }
};
} // namespace winx_remix
