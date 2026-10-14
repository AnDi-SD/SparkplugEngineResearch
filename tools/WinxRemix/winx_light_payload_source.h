#pragma once
// Own evaluator of observed PC light fields. Shared recovered PC4B53C0 supplies
// arithmetic; this class neither runs native producers nor changes game bytes.
// CachedEquivalent means consumed-word equality, not current world/controller
// dependencies, an RT freshness proof, or complete physical-light validity.
#include "winx_scene_light_snapshot.h"
#include "../../Sparkplug/Code/SparkplugDX/spPCLightPayload.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace winx_remix {
class LightPayloadSource final {
public:
  using Payload = std::array<std::uint32_t, 26>;
  enum class Status { Invalid, Dirty, CachedMismatch, CachedEquivalent };
  struct Result {
    Status status = Status::Invalid;
    std::uint32_t type = 0, flags = 0, worldDirtyMask = 0, consumedMask = 0, differenceMask = 0;
    bool lightDataDirty = false;
    Payload cached{}, candidate{};

    // Candidate is diagnostic arithmetic from observed cached world inputs.
    // It is available even when its consumed words differ from native cache.
    bool CandidateAvailable() const noexcept {
      return status == Status::CachedMismatch || status == Status::CachedEquivalent;
    }
    // Preserve every unconsumed native word. A failed qualification leaves the
    // caller's output untouched; candidate alone never authorizes a source switch.
    bool CopyCachedEquivalentPayload(Payload& output) const noexcept {
      if (status != Status::CachedEquivalent) return false;
      Payload selected = cached;
      for (unsigned i = 0; i < 26; ++i) if (consumedMask & (1u << i)) selected[i] = candidate[i];
      output = selected;
      return true;
    }
  };

  static constexpr std::uint32_t ConsumedMask(std::uint32_t type) noexcept {
    // Type/diffuse RGB, plus the current converter's type-specific fields.
    // Diffuse alpha, specular/ambient and point direction are not consumed.
    return type == 0 ? 0x7000Fu : type == 1 ? 0xE8E00Fu : type == 2 ? 0x3FFE00Fu : 0;
  }

  Result Evaluate(const SceneLightSnapshot::Record& record) const noexcept {
    return Evaluate(record.address, record.raw);
  }
  Result Evaluate(std::uint32_t address, const unsigned char (&raw)[0x158]) const noexcept {
    Result result;
    result.type = At(raw, 0xc0);
    result.flags = At(raw, 0xb0);
    result.worldDirtyMask = result.flags & 7u;
    result.lightDataDirty = (result.flags & 8u) != 0;
    // Preserve the existing reader's ordering: structural invalidity precedes
    // Dirty, while shared arithmetic finite-input validation follows Dirty.
    if (!address || At(raw, 0) != 0x6f0c88 || !At(raw, 0x3c) || result.type > 2 ||
        raw[0xd4] > 1 || raw[0xed] != 1 || !(result.flags & 0x100)) return result;
    if (result.flags & 0xfu) { result.status = Status::Dirty; return result; }

    sparkplug::reconstruction::PCLightPayloadInputsForAnalysis input{};
    input.kind = result.type;
    input.attenuationEnabled = raw[0xd4] != 0;
    std::memcpy(input.color.data(), raw + 0xc4, 16);
    std::memcpy(input.position.data(), raw + 0x74, 12);
    std::memcpy(input.direction.data(), raw + 0xa4, 12);
    std::memcpy(&input.intensity, raw + 0xd8, 4);
    std::memcpy(&input.range, raw + 0xe0, 4);
    std::memcpy(&input.hotspot, raw + 0xe4, 4);
    std::memcpy(&input.falloff, raw + 0xe8, 4);
    // Zero defaultVector/ambient are explicit scratch, not inferred native
    // globals. They affect only words excluded by the current consumed masks.
    std::memcpy(result.cached.data(), raw + 0xf0, sizeof(result.cached));
    if (!sparkplug::reconstruction::RefreshPCLightPayloadForAnalysis(input, result.candidate)) return result;

    result.consumedMask = ConsumedMask(result.type);
    for (unsigned i = 0; i < 26; ++i)
      if ((result.consumedMask & (1u << i)) && result.candidate[i] != result.cached[i])
        result.differenceMask |= 1u << i;
    result.status = result.differenceMask ? Status::CachedMismatch : Status::CachedEquivalent;
    return result;
  }

private:
  static std::uint32_t At(const unsigned char* bytes, std::size_t offset) noexcept {
    std::uint32_t result;
    std::memcpy(&result, bytes + offset, sizeof(result));
    return result;
  }
};
} // namespace winx_remix
