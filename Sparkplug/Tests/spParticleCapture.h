#pragma once
#include "Code/Sparkplug/spParticleSystem.h"

namespace sparkplug::reconstruction::particle_test
{
    inline std::vector<std::uint8_t> State(const spParticleSystem& object)
    {
        std::vector<std::uint8_t> result;
        const auto add=[&](const auto& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);result.insert(result.end(),p,p+sizeof(value));};
        add(std::uint8_t(object.IsAlphaSortEnabledForAnalysis()));add(object.GetPriorityForAnalysis());
        const auto& p=object.Parameters();add(p.flags);add(p.rate);add(p.times);add(p.colors);
        add(p.acceleration);add(p.direction);add(p.velocity);add(p.angle);add(p.scale);add(p.sphere);add(p.regionType);
        for(float value:p.region)add(value);add(object.GetPoolStateForAnalysis());return result;
    }
}
