#pragma once
// Own narrow native material recipe reader. Shared recovered PC mappings are
// the only state equations used here. Capture copies values; it does not call
// producers, retain native objects, qualify their lifetime, bind a texture,
// infer stage arguments, or grant permission to draw.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../../../../Sparkplug/Analysis/PC/SparkplugAbi.h"
#include "../../../../Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h"
#include "../../../../Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h"

namespace winx_remix {
class MaterialRecipeSource final {
public:
  enum class Reason { Material, Controllers, Pass, Texture, Override, Mapping };
  struct Request {
    std::uint32_t renderer = 0, material = 0, device = 0;
    std::uint8_t outerOverride = 0;
  };
  struct Recipe {
    bool valid = false;
    Reason reason = Reason::Material; // Meaningful only when valid is false.
    // Copied addresses are operation-local provenance, not resource IDs or
    // ownership/lifetime tokens. No native pointer is retained in this value.
    std::uint32_t renderer = 0, material = 0, pass = 0, layer = 0;
    std::uint32_t textureOwner = 0, texture = 0, textureCOM = 0;
    std::uint8_t effectiveOverride = 0;
    std::array<std::uint32_t, 9> rawTexture{};
    std::array<std::uint32_t, 11> rawMaterial{};
    std::array<std::uint32_t, 29> stage{};
    std::array<std::uint32_t, 14> sampler{};
    std::array<std::uint32_t, 256> mappedStates{};
    std::array<std::uint8_t, 256> mappedPresent{};
  };

  // reader(address, destination, byteCount) copies the requested native POD
  // bytes and returns success. The adapter supplies safe reads and surrounding
  // scene/update/renderer/binding fences. Failed reads never publish a recipe.
  template<class Reader>
  static Recipe Capture(const Request& request, const Reader& reader) {
    namespace abi = sparkplug::evidence::pc;
    abi::spDXMaterialObservedLayout material{};
    if (!request.material || !Read(reader, request.material, material) ||
        material.base.base.vtableAddress != abi::spDXMaterialPrimaryVTable ||
        material.base.materialVTable != abi::spDXMaterialInterfaceVTable ||
        material.base.renderStates[8] != 2) return Rejected(Reason::Material);
    if (material.base.materialColorController) return Rejected(Reason::Controllers);
    abi::spMaterialPassLayerObservedLayout pass{};
    if (material.base.passCount != 1 || !Read(reader, material.base.passes[0], pass) ||
        pass.base.vtableAddress != abi::spMaterialPassLayerVTable ||
        pass.layerCount != 1 || pass.finalBlendOperation) return Rejected(Reason::Pass);
    abi::spStdLayerObservedLayout layer{};
    abi::spMaterialTextureObservedLayout holder{};
    if (!Read(reader, pass.layers[0], layer) ||
        layer.base.base.vtableAddress != abi::spStdLayerVTable ||
        !Read(reader, layer.base.materialTexture, holder) ||
        holder.base.vtableAddress != abi::spMaterialTextureVTable) return Rejected(Reason::Texture);
    if (holder.uvController || holder.animationController || holder.hasStaticUV)
      return Rejected(Reason::Controllers);
    abi::spDXTextureObservedLayout texture{};
    if (!Read(reader, holder.fallbackTexture, texture) ||
        texture.base.base.base.base.vtableAddress != abi::spDXTexturePrimaryVTable ||
        texture.device != request.device || !texture.texture) return Rejected(Reason::Texture);

    Recipe output;
    output.effectiveOverride = material.base.renderOverride ? 0 : request.outerOverride;
    if (output.effectiveOverride) {
      std::uint32_t selectors[11]{};
      if (!Read(reader, request.renderer + 0xc71c, selectors)) return Rejected(Reason::Override);
      for (unsigned index = 1; index < 11; ++index)
        if (selectors[index]) return Rejected(Reason::Override);
    }

    TextureCache cache;
    output.stage.fill(~0u);
    output.sampler.fill(~0u);
    TextureMapping mapping{output};
    for (unsigned index = 1; index < 9; ++index)
      if (!sparkplug::reconstruction::ApplyPCTextureStateForAnalysis(
            cache, 0, index, holder.textureStates[index], false, MapTexture, &mapping))
        return Rejected(Reason::Mapping);
    if (!mapping.valid || output.stage[11] || output.stage[24]) return Rejected(Reason::Mapping);

    output.renderer = request.renderer;
    output.material = request.material;
    output.pass = material.base.passes[0];
    output.layer = pass.layers[0];
    output.textureOwner = layer.base.materialTexture;
    output.texture = holder.fallbackTexture;
    output.textureCOM = texture.texture;
    std::memcpy(output.rawTexture.data(), holder.textureStates, sizeof(holder.textureStates));
    std::memcpy(output.rawMaterial.data(), material.base.renderStates, sizeof(material.base.renderStates));
    // Original pass application writes the material word. This reader changes
    // only its own copy, preserving the pre-render native material bytes.
    output.rawMaterial[7] = pass.finalBlendOperation;
    std::array<std::uint32_t, 11> raw{};
    raw.fill(~0u); // Deliberate initial misses describe the expected emitted set.
    Lighting lighting;
    // Mode2 does not consume the native material colors, power, packed color
    // or global black. Those values are not read or inferred for this cohort.
    if (!sparkplug::reconstruction::ApplyPCMaterialStateSetForAnalysis(
          raw, output.rawMaterial, static_cast<const Overrides*>(nullptr), lighting,
          MapMaterial, &output)) return Rejected(Reason::Mapping);
    output.valid = true;
    return output;
  }

private:
  struct TextureCache {
    std::array<std::uint32_t, 9> raw{};
    std::uint32_t coordinateIndex = ~0u, transformFlags = ~0u;
  };
  struct TextureMapping { Recipe& output; bool valid = true; };
  struct Lighting {
    std::array<float, 4> diffuse{}, ambient{}, specular{}, emissive{};
    float specularPower = 0;
    std::uint32_t packedColorC194 = 0, globalBlackARGB = 0;
    std::uint32_t diffuseSource = ~0u, ambientSource = ~0u;
  };
  struct Overrides {
    const std::array<std::uint32_t, 11>* const* sources = nullptr;
    std::size_t count = 0;
    std::array<std::uint32_t, 10> selectors{};
  };
  static Recipe Rejected(Reason reason) {
    Recipe output;
    output.reason = reason;
    return output;
  }
  template<class Reader, class Value>
  static bool Read(const Reader& reader, std::uint32_t address, Value& value) {
    return reader(address, &value, sizeof(value));
  }
  static std::int32_t MapTexture(void* context, bool sampler, std::uint32_t stage,
                                 std::uint32_t index, std::uint32_t value) noexcept {
    auto& mapping = *static_cast<TextureMapping*>(context);
    auto& output = mapping.output;
    if (stage || index >= (sampler ? output.sampler.size() : output.stage.size())) {
      mapping.valid = false;
      return -1;
    }
    if (sampler) output.sampler[index] = value;
    else output.stage[index] = value;
    return 0;
  }
  static std::int32_t MapMaterial(void* context, std::uint32_t index,
                                  std::uint32_t value) noexcept {
    auto& output = *static_cast<Recipe*>(context);
    if (index >= output.mappedStates.size()) return -1;
    output.mappedStates[index] = value;
    output.mappedPresent[index] = 1;
    return 0;
  }
};
} // namespace winx_remix
