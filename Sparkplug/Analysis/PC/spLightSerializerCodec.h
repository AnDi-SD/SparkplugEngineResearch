#pragma once
// Shared analytical adapter for the independently identical PC440640/471670
// and440110/471AF0 grammars. Not an original RTTI class.
#include "Code/Sparkplug/spLight.h"
#include "Code/Sparkplug/spSerializer.h"
namespace sparkplug::evidence::pc::serialization
{
    bool ReadLightFields(reconstruction::spSerializerReadContextForAnalysis&,
        reconstruction::spStream&,std::uint32_t,reconstruction::spLight&,std::string*);
    bool WriteLightFields(reconstruction::spStream&,const reconstruction::spLight&,
        std::uint32_t defaultWhiteARGB,std::string*);
    std::vector<std::uint32_t> LightWriteFields(const reconstruction::spLight&,
        std::uint32_t defaultWhiteARGB);
}
