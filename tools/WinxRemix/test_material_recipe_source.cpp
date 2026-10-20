// Own portable CPU fixture. Fixed numeric addresses name owned test records;
// no game instructions, native objects, COM, D3D, or Remix API are executed.
#include "winx_material_recipe_source.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

using Source = winx_remix::MaterialRecipeSource;
namespace abi = sparkplug::evidence::pc;
static unsigned checks;
static void Check(bool value, const char* name) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static_assert(std::is_trivially_copyable<Source::Recipe>::value,
              "recipe owns plain values, with no dynamic resource ownership");
struct Fixture {
  static constexpr std::uint32_t rendererAddress = 0x100000;
  static constexpr std::uint32_t materialAddress = 0x200000;
  static constexpr std::uint32_t passAddress = 0x300000;
  static constexpr std::uint32_t layerAddress = 0x400000;
  static constexpr std::uint32_t holderAddress = 0x500000;
  static constexpr std::uint32_t textureAddress = 0x600000;
  static constexpr std::uint32_t selectorAddress = rendererAddress + 0xc71c;
  Source::Request request{rendererAddress, materialAddress, 0x123400, 0};
  abi::spDXMaterialObservedLayout material{};
  abi::spMaterialPassLayerObservedLayout pass{};
  abi::spStdLayerObservedLayout layer{};
  abi::spMaterialTextureObservedLayout holder{};
  abi::spDXTextureObservedLayout texture{};
  std::uint32_t selectors[11]{};
  std::uint32_t unreadable = ~0u, partialRead = ~0u;
  mutable std::vector<std::uint32_t> reads;
  Fixture() {
    material.base.base.vtableAddress = abi::spDXMaterialPrimaryVTable;
    material.base.materialVTable = abi::spDXMaterialInterfaceVTable;
    material.base.passCount = 1;
    material.base.passes[0] = passAddress;
    const std::uint32_t states[11] = {~0u, 1, 1, 2, 1, 1, 7, 9, 2, 17, 7};
    std::memcpy(material.base.renderStates, states, sizeof(states));
    pass.base.vtableAddress = abi::spMaterialPassLayerVTable;
    pass.layerCount = 1;
    pass.layers[0] = layerAddress;
    layer.base.base.vtableAddress = abi::spStdLayerVTable;
    layer.base.materialTexture = holderAddress;
    holder.base.vtableAddress = abi::spMaterialTextureVTable;
    holder.fallbackTexture = textureAddress;
    const std::uint32_t textureStates[9] = {0xabcdef, 3, 6, 0, 1, 2, 3, 0, 0};
    std::memcpy(holder.textureStates, textureStates, sizeof(textureStates));
    texture.base.base.base.base.vtableAddress = abi::spDXTexturePrimaryVTable;
    texture.device = request.device;
    texture.texture = 0x234500;
  }
  bool Read(std::uint32_t address, void* destination, std::size_t count) const {
    reads.push_back(address);
    if (address == unreadable) return false;
    const void* source = nullptr;
    std::size_t available = 0;
    switch (address) {
    case materialAddress: source = &material; available = sizeof(material); break;
    case passAddress: source = &pass; available = sizeof(pass); break;
    case layerAddress: source = &layer; available = sizeof(layer); break;
    case holderAddress: source = &holder; available = sizeof(holder); break;
    case textureAddress: source = &texture; available = sizeof(texture); break;
    case selectorAddress: source = selectors; available = sizeof(selectors); break;
    default: return false;
    }
    if (count != available) return false; // Fixture supplies complete POD records only.
    if (address == partialRead) {
      std::memcpy(destination, source, count / 2);
      return false;
    }
    std::memcpy(destination, source, count);
    return true;
  }
  Source::Recipe Capture() const {
    reads.clear();
    return Source::Capture(request, [this](std::uint32_t address, void* value, std::size_t size) {
      return Read(address, value, size);
    });
  }
};
template<class Value> static bool Zeros(const Value& values) {
  for (const auto value : values) if (value) return false;
  return true;
}
static void Failure(const Fixture& fixture, Source::Reason reason, std::size_t readCount,
                    const char* name) {
  const auto recipe = fixture.Capture();
  Check(!recipe.valid && recipe.reason == reason, name);
  Check(!recipe.renderer && !recipe.material && !recipe.pass && !recipe.layer &&
        !recipe.textureOwner && !recipe.texture && !recipe.textureCOM && !recipe.effectiveOverride &&
        Zeros(recipe.rawTexture) && Zeros(recipe.rawMaterial) && Zeros(recipe.stage) &&
        Zeros(recipe.sampler) && Zeros(recipe.mappedStates) && Zeros(recipe.mappedPresent),
        "a rejection publishes no partial recipe");
  Check(fixture.reads.size() == readCount, "rejection stops at its original read boundary");
}
static void Ordinary() {
  Fixture fixture;
  const auto material = fixture.material;
  const auto pass = fixture.pass;
  const auto layer = fixture.layer;
  const auto holder = fixture.holder;
  const auto texture = fixture.texture;
  const auto result = fixture.Capture();
  Check(result.valid, "ordinary one-pass one-StdLayer mode2 raw recipe accepted");
  Check(fixture.reads == std::vector<std::uint32_t>{Fixture::materialAddress, Fixture::passAddress,
        Fixture::layerAddress, Fixture::holderAddress, Fixture::textureAddress},
        "native POD reads preserve material pass layer holder texture order");
  Check(result.renderer == fixture.request.renderer && result.material == fixture.request.material &&
        result.pass == Fixture::passAddress && result.layer == Fixture::layerAddress &&
        result.textureOwner == Fixture::holderAddress && result.texture == Fixture::textureAddress &&
        result.textureCOM == 0x234500, "operation-local numeric provenance copied");
  Check(std::memcmp(result.rawTexture.data(), holder.textureStates, sizeof(holder.textureStates)) == 0,
        "all nine raw texture words including unapplied index zero copied");
  Check(result.rawMaterial[0] == ~0u && result.rawMaterial[7] == 0 &&
        fixture.material.base.renderStates[7] == 9, "pass final blend changes only owned material word seven");
  for (unsigned index = 1; index < 11; ++index)
    if (index != 7) Check(result.rawMaterial[index] == material.base.renderStates[index],
                         "remaining raw material words preserved");
  Check(result.stage[1] == 4 && result.stage[4] == 7 && result.stage[11] == 0 && result.stage[24] == 0,
        "literal recovered texture operation and UV mappings");
  Check(result.sampler[1] == 1 && result.sampler[2] == 2 && result.sampler[4] == 2 &&
        result.sampler[5] == 3 && result.sampler[6] == 3 && result.sampler[7] == 2,
        "literal recovered address border and filter mappings");
  Check(result.stage[2] == ~0u && result.stage[3] == ~0u && result.stage[5] == ~0u &&
        result.stage[6] == ~0u && result.stage[28] == ~0u && result.sampler[13] == ~0u,
        "unmapped texture arguments and sampler slots stay explicitly absent");
  const std::uint32_t indices[] = {8, 9, 22, 7, 14, 23, 19, 20, 29, 137, 145, 148, 24, 25};
  const std::uint32_t values[] = {2, 2, 3, 1, 1, 8, 2, 1, 0, 0, 1, 0, 17, 8};
  for (unsigned i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i)
    Check(result.mappedPresent[indices[i]] && result.mappedStates[indices[i]] == values[i],
          "literal full expected material state emitted from fresh local cache");
  unsigned emitted = 0;
  for (auto present : result.mappedPresent) emitted += present != 0;
  Check(emitted == 14 && !result.mappedPresent[27] && !result.mappedPresent[60],
        "unmapped alpha enable and texture factor are not inferred");
  Check(std::memcmp(&material, &fixture.material, sizeof(material)) == 0 &&
        std::memcmp(&pass, &fixture.pass, sizeof(pass)) == 0 &&
        std::memcmp(&layer, &fixture.layer, sizeof(layer)) == 0 &&
        std::memcmp(&holder, &fixture.holder, sizeof(holder)) == 0 &&
        std::memcmp(&texture, &fixture.texture, sizeof(texture)) == 0,
        "reading and mapping never modify any source record");
}
static void Gates() {
  { Fixture f; f.request.material = 0; Failure(f, Source::Reason::Material, 0, "missing material has no invented fallback"); }
  { Fixture f; f.material.base.base.vtableAddress = 1; Failure(f, Source::Reason::Material, 1, "unknown material primary rejected"); }
  { Fixture f; f.material.base.materialVTable = 1; Failure(f, Source::Reason::Material, 1, "unknown material interface rejected"); }
  for (auto mode : {0u, 1u, 3u, 4u, 5u, 6u, 7u, 8u, ~0u}) {
    Fixture f; f.material.base.renderStates[8] = mode;
    Failure(f, Source::Reason::Material, 1, "lighting modes outside exact mode2 rejected");
  }
  { Fixture f; f.material.base.materialColorController = 1; f.material.base.passCount = 2;
    Failure(f, Source::Reason::Controllers, 1, "color controller rejected before pass gate"); }
  for (auto count : {0u, 2u, 8u, ~0u}) {
    Fixture f; f.material.base.passCount = count;
    Failure(f, Source::Reason::Pass, 1, "non-single pass count rejected before pass read");
  }
  { Fixture f; f.material.base.passes[0] = 0; Failure(f, Source::Reason::Pass, 2, "missing pass is not silently skipped"); }
  { Fixture f; f.pass.base.vtableAddress = 1; Failure(f, Source::Reason::Pass, 2, "unknown pass subtype rejected"); }
  { Fixture f; f.pass.layerCount = 2; Failure(f, Source::Reason::Pass, 2, "multi-layer pass rejected"); }
  { Fixture f; f.pass.finalBlendOperation = 1; Failure(f, Source::Reason::Pass, 2, "nonzero pass final blend rejected"); }
  { Fixture f; f.pass.layers[0] = 0; Failure(f, Source::Reason::Texture, 3, "missing layer rejected"); }
  { Fixture f; f.layer.base.base.vtableAddress = 1; Failure(f, Source::Reason::Texture, 3, "unknown layer subtype prevents holder read"); }
  { Fixture f; f.layer.base.materialTexture = 0; Failure(f, Source::Reason::Texture, 4, "missing holder rejected"); }
  { Fixture f; f.holder.base.vtableAddress = 1; Failure(f, Source::Reason::Texture, 4, "derived unknown getter path rejected"); }
  { Fixture f; f.holder.uvController = 1; f.holder.fallbackTexture = 0;
    Failure(f, Source::Reason::Controllers, 4, "UV controller excluded before texture binding recipe"); }
  { Fixture f; f.holder.animationController = 1; Failure(f, Source::Reason::Controllers, 4, "animated texture source excluded"); }
  { Fixture f; f.holder.hasStaticUV = 1; Failure(f, Source::Reason::Controllers, 4, "static transform source excluded"); }
  { Fixture f; f.holder.fallbackTexture = 0; Failure(f, Source::Reason::Texture, 5, "NULL fallback rejected without calling virtual getter"); }
  { Fixture f; f.texture.base.base.base.base.vtableAddress = 1; Failure(f, Source::Reason::Texture, 5, "unknown native texture branch rejected"); }
  { Fixture f; f.texture.device += 1; Failure(f, Source::Reason::Texture, 5, "foreign device texture rejected"); }
  { Fixture f; f.texture.texture = 0; Failure(f, Source::Reason::Texture, 5, "empty texture transport rejected"); }
  const std::uint32_t addresses[] = {Fixture::materialAddress, Fixture::passAddress, Fixture::layerAddress,
                                   Fixture::holderAddress, Fixture::textureAddress};
  const Source::Reason reasons[] = {Source::Reason::Material, Source::Reason::Pass, Source::Reason::Texture,
                                    Source::Reason::Texture, Source::Reason::Texture};
  for (unsigned index = 0; index < 5; ++index) {
    Fixture f; f.unreadable = addresses[index];
    Failure(f, reasons[index], index + 1, "unreadable source record refuses recipe at exact boundary");
    f.unreadable = ~0u; f.partialRead = addresses[index];
    Failure(f, reasons[index], index + 1, "failed partial POD read cannot leak partial output");
  }
}
static void Overrides() {
  { Fixture f; f.unreadable = Fixture::selectorAddress;
    Check(f.Capture().valid && f.reads.size() == 5, "disabled outer override does not read selectors"); }
  { Fixture f; f.request.outerOverride = 1; f.unreadable = Fixture::selectorAddress;
    Failure(f, Source::Reason::Override, 6, "effective override requires readable selectors"); }
  { Fixture f; f.request.outerOverride = 1; f.partialRead = Fixture::selectorAddress;
    Failure(f, Source::Reason::Override, 6, "partial selector read rejected"); }
  for (unsigned index = 1; index < 11; ++index) {
    Fixture f; f.request.outerOverride = 1; f.selectors[index] = 1;
    Failure(f, Source::Reason::Override, 6, "every consumed nonzero selector rejects alternate source");
  }
  { Fixture f; f.request.outerOverride = 1; f.selectors[0] = ~0u;
    const auto result = f.Capture();
    Check(result.valid && result.effectiveOverride == 1 && f.reads.back() == Fixture::selectorAddress,
          "selector zero is not consumed and complete source-zero override is supported"); }
  { Fixture f; f.request.outerOverride = 255;
    Check(f.Capture().effectiveOverride == 255, "outer override byte preserved without bool normalization"); }
  for (auto overrideValue : {1u, 255u}) {
    Fixture f; f.request.outerOverride = 1; f.material.base.renderOverride = static_cast<std::uint8_t>(overrideValue);
    f.unreadable = Fixture::selectorAddress;
    const auto result = f.Capture();
    Check(result.valid && !result.effectiveOverride && f.reads.size() == 5,
          "any nonzero material render override clears effective flag without native Pre");
  }
  { Fixture f; f.request.outerOverride = 1; f.texture.texture = 0; f.unreadable = Fixture::selectorAddress;
    Failure(f, Source::Reason::Texture, 5, "texture gate precedes selector gate"); }
  { Fixture f; f.request.outerOverride = 1; f.selectors[2] = 1; f.holder.textureStates[1] = 16;
    Failure(f, Source::Reason::Override, 6, "override gate precedes state mapping gate"); }
}
static void Mapping() {
  for (auto index : {1u, 2u}) {
    Fixture f; f.holder.textureStates[index] = 16;
    Failure(f, Source::Reason::Mapping, 5, "out-of-range operation table input rejected by shared mapping");
  }
  { Fixture f; f.holder.textureStates[7] = 1;
    Failure(f, Source::Reason::Mapping, 5, "nonzero UV coordinate source excluded"); }
  for (auto flags : {1u, 2u, 3u, 4u, 8u}) {
    Fixture f; f.holder.textureStates[8] = flags;
    Failure(f, Source::Reason::Mapping, 5, "transformed texture coordinates excluded");
  }
  const unsigned badIndex[] = {2, 3, 6, 10};
  const std::uint32_t badValue[] = {2, 3, 8, 8};
  for (unsigned i = 0; i < 4; ++i) {
    Fixture f; f.material.base.renderStates[badIndex[i]] = badValue[i];
    Failure(f, Source::Reason::Mapping, 5, "shared material table guard retained");
  }
  { Fixture f; f.holder.textureStates[1] = 8;
    const auto result = f.Capture();
    Check(result.valid && result.stage[1] == 13,
          "raw recipe does not invent the adapter's supported RGB equation policy"); }
  { Fixture f; f.holder.textureStates[7] = 99; f.holder.textureStates[8] = 5;
    const auto result = f.Capture();
    Check(result.valid && result.stage[11] == 0 && result.stage[24] == 0 &&
          result.rawTexture[7] == 99 && result.rawTexture[8] == 5,
          "recovered mapping fallback values preserved as raw and mapped values"); }
  { Fixture f; f.request.device = 0; f.texture.device = 0; f.request.renderer = 0;
    Check(f.Capture().valid, "context pointer validation remains the adapter's responsibility"); }
  { Fixture f; f.material.diffuseRGBA[0] = std::numeric_limits<float>::quiet_NaN();
    f.material.specularPowerBits = ~0u; f.holder.uvTransform[0] = std::numeric_limits<float>::infinity();
    Check(f.Capture().valid, "unconsumed mode2 colors power and inactive UV values do not create extra gates"); }
}
static void OwnedValues() {
  Fixture fixture;
  const auto first = fixture.Capture();
  fixture.material.base.renderStates[9] = 43;
  fixture.holder.textureStates[3] = 2;
  fixture.texture.texture = 0x777700;
  const auto second = fixture.Capture();
  Check(first.valid && second.valid && first.rawMaterial[9] == 17 && second.rawMaterial[9] == 43 &&
        first.mappedStates[24] == 17 && second.mappedStates[24] == 43,
        "successive captures own independent raw and mapped material values");
  Check(first.rawTexture[3] == 0 && first.sampler[1] == 1 && second.rawTexture[3] == 2 &&
        second.sampler[1] == 3 && first.textureCOM == 0x234500 && second.textureCOM == 0x777700,
        "later source updates do not mutate earlier texture recipe");
  auto changed = second;
  changed.rawTexture[3] = 99; changed.mappedPresent[24] = 0;
  Check(second.rawTexture[3] == 2 && second.mappedPresent[24] == 1,
        "copied recipes do not alias each other's value arrays");
  fixture.material.base.renderStates[2] = 2;
  Failure(fixture, Source::Reason::Mapping, 5, "failed capture cannot return previous successful recipe");
  Check(first.valid && first.rawMaterial[2] == 1 && second.valid && second.rawMaterial[2] == 1,
        "rejection leaves previously returned owned values unchanged");
}
int main() {
  Ordinary(); Gates(); Overrides(); Mapping(); OwnedValues();
  std::printf("{\"status\":\"PASS\",\"checks\":%u}\n", checks);
  return 0;
}
