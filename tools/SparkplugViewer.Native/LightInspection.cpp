#include "LightInspection.h"
#include "Analysis/PC/spLightSerializerCodec.h"
#include "Code/Sparkplug/spLightData.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace spvhost {
LightInspection ReadLightInspection(const std::uint8_t* bytes, std::uint32_t size) {
    using namespace sparkplug::reconstruction;
    if (!bytes || !size || size > 16u * 1024u * 1024u)
        throw std::runtime_error("Invalid bounded Light inspection section");
    spMemoryStream stream;
    if (!stream.ResizeAndSetSize(size))
        throw std::runtime_error("Cannot allocate Light inspection stream");
    std::memcpy(stream.GetBuffer(), bytes, size);
    if (!stream.Seek(spStream::SeekSource::essStart, 0))
        throw std::runtime_error("Cannot rewind Light inspection stream");
    spLightData light;
    spSerializerManager manager;
    spResourceManager resources;
    spSerializerReadContextForAnalysis context(manager, resources);
    std::string error;
    if (!sparkplug::evidence::pc::serialization::ReadLightFields(context, stream, size, light, &error))
        throw std::runtime_error(error.empty() ? "Cannot read Light inspection section" : error);
    LightInspection output{};
    output.type = static_cast<std::uint32_t>(light.GetTypeForAnalysis());
    output.projectShadow = light.ProjectsShadowVolumeForAnalysis() ? 1u : 0u;
    output.attenuation = light.UsesAttenuationForAnalysis() ? 1u : 0u;
    output.enabled = light.IsLightEnabledForAnalysis() ? 1u : 0u;
    std::copy(light.GetColorForAnalysis().begin(), light.GetColorForAnalysis().end(), output.color);
    output.intensity = light.GetIntensityForAnalysis();
    output.range = light.GetRangeForAnalysis();
    output.hotspot = light.GetHotspotAngleForAnalysis();
    output.falloff = light.GetFalloffAngleForAnalysis();
    return output;
}
}
