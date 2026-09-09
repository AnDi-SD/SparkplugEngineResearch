#pragma once
// Host projection of the actual shared light reader. This is an inspection
// DTO, not an original engine class or a loaded scene/light-manager binding.
#include <cstdint>

namespace spvhost {
struct LightInspection {
    std::uint32_t type, projectShadow, attenuation, enabled;
    float color[4];
    float intensity, range, hotspot, falloff;
};
static_assert(sizeof(LightInspection) == 48);

// Input is the complete own light section, including its section terminator.
// Throws std::runtime_error on a bounded input or shared-reader failure.
[[nodiscard]] LightInspection ReadLightInspection(const std::uint8_t*, std::uint32_t);
}
