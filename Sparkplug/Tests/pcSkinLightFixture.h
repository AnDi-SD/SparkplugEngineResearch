#pragma once
#include "Code/Sparkplug/spLightDataSerializer.h"
#include "Code/SparkplugDX/spDXLight.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace sparkplug::tests::skin
{
    inline std::unique_ptr<reconstruction::spDXLight> ReadLight(const std::string& input,std::string& capture)
    {
        using namespace reconstruction;spMemoryStream stream;
        if(input.size()%2||input.size()<20||input.size()>200||!stream.ResizeAndSetSize(static_cast<unsigned>(input.size()/2)))throw std::runtime_error("bounded compact light input");
        auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
        for(std::size_t i=0;i<input.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(input.substr(i,2),nullptr,16));
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spLightDataSerializer serializer;
        auto owner=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);auto* light=dynamic_cast<spDXLight*>(owner.get());std::string error;
        if(!light)throw std::runtime_error("LightData header did not create DXLight");
        if(!(light->GetFlagsForAnalysis()&8u))throw std::runtime_error("native Light constructor dirty8");
        light->SetOpaqueRuntimeFieldBitsForAnalysis(0xa1b2c3d4);light->SetWorldDeviceInputsForAnalysis({.125F,.25F,.5F},0x7f234567);
        if(!serializer.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(input.size()/2-8),*light,&error))throw std::runtime_error(error);
        unsigned cursor=0;if(!stream.GetCurrentPosition(cursor)||cursor!=input.size()/2||light->GetChildCountForAnalysis())throw std::runtime_error("complete scalar light slice required");
        const auto bits=[](float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;};
        std::ostringstream out;out<<"[["<<static_cast<unsigned>(light->GetTypeForAnalysis());for(float v:light->GetColorForAnalysis())out<<','<<bits(v);
        out<<','<<light->ProjectsShadowVolumeForAnalysis()<<','<<light->UsesAttenuationForAnalysis()<<','<<light->IsLightEnabledForAnalysis();
        for(float v:{light->GetIntensityForAnalysis(),light->GetRangeForAnalysis(),light->GetHotspotAngleForAnalysis(),light->GetFalloffAngleForAnalysis()})out<<','<<bits(v);
        out<<','<<light->GetOpaqueRuntimeFieldBitsForAnalysis()<<','<<light->GetFlagsForAnalysis()<<"],\"";
        const char* digits="0123456789abcdef";const auto emit=[&](const auto& values){const auto* p=reinterpret_cast<const unsigned char*>(values.data());for(std::size_t i=0;i<sizeof(values);++i)out<<digits[p[i]>>4]<<digits[p[i]&15];};
        emit(light->GetPositionForAnalysis());emit(light->GetScaleForAnalysis());emit(light->GetOrientationForAnalysis());emit(light->GetWorldPositionForAnalysis());emit(light->GetWorldScaleForAnalysis());emit(light->GetWorldOrientationForAnalysis());
        out<<"\"]";capture=out.str();return std::unique_ptr<spDXLight>(static_cast<spDXLight*>(owner.release()));
    }
}
