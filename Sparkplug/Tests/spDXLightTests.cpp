#include "Code/SparkplugDX/spDXLight.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    std::string Snapshot(const spDXLight& light)
    {std::ostringstream o;o<<'['<<Bits(light.GetIntensityForAnalysis())<<",[";const auto& values=light.GetDevicePayloadForAnalysis();for(unsigned i=0;i<26;++i){if(i)o<<',';o<<values[i].value_or(0xcccccccc);}o<<"]]";return o.str();}
    std::string Run(const std::string& mode)
    {
        spDXLight light;Check(light.IsKindOf(spLight::ClassID),"original hierarchy");
        const spNode::Vector3 position{4,5,6},direction{1,2,3},defaultVector{.125F,.25F,.5F};
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        if(mode=="factory")out<<Snapshot(light);
        else if(mode=="copy"||mode=="clone")
        {
            light.SetIntensityForAnalysis(3.25F);spDXLight::DevicePayloadForAnalysis values{};for(unsigned i=0;i<26;++i)values[i]=0x10000000+i;light.SetDevicePayloadForAnalysis(values);
            if(mode=="copy"){spDXLight destination;destination.SetIntensityForAnalysis(9.5F);Check(light.CopyIntoForAnalysis(destination),"copy");out<<Snapshot(destination);}
            else{auto owner=light.Clone();auto* clone=dynamic_cast<spDXLight*>(owner.get());Check(clone!=nullptr,"actual allocating clone");out<<Snapshot(*clone);}
        }
        else if(mode=="world")
        {
            // Native executes full no-scene4B58D0. Source compares its refresh
            // predicate/payload with explicit already-updated world inputs;
            // this is not a reconstructed light scene/world callback chain.
            unsigned iteration=0;
            for(const auto& event:std::array<std::array<unsigned,3>,5>{{{0,0,1},{8,0,1},{0,8,1},{0,8,0},{1,0,1}}})
            {
                light.SetDevicePayloadForAnalysis({});const bool refresh=spDXLight::NeedsDeviceRefreshForAnalysis(event[1],event[0],event[2]!=0);
                if(refresh)Check(light.RefreshDevicePayloadForAnalysis(position,event[0]==1?spNode::Vector3{}:direction,defaultVector,0x7f234567),"refresh predicate consumer");
                if(iteration++)out<<',';out<<'['<<event[0]<<','<<event[1]<<','<<event[2]<<','<<(refresh?"true":"false")<<','<<Snapshot(light)<<']';
            }
        }
        else
        {
            const unsigned kind=mode=="directional"?0:mode=="point"?1:mode=="spot"?2:3;
            light.SetTypeForAnalysis(static_cast<spLight::Type>(kind));unsigned iteration=0;
            for(const auto& input:std::array<std::array<float,3>,6>{{{0,200,1},{1,200,2},{0,10,-2},{1,0,2},{1,200,0},{1,-5,3}}})
            {
                light.SetDevicePayloadForAnalysis({});light.SetUsesAttenuationForAnalysis(input[0]!=0);light.SetRangeForAnalysis(input[1]);light.SetIntensityForAnalysis(input[2]);light.SetColorForAnalysis({.25F,.75F,1.5F,.5F});light.SetHotspotAngleForAnalysis(.5F);light.SetFalloffAngleForAnalysis(1);
                Check(light.RefreshDevicePayloadForAnalysis(position,direction,defaultVector,0x7f234567),"finite device payload");
                if(iteration++)out<<',';out<<'['<<unsigned(input[0])<<','<<Bits(input[1])<<','<<Bits(input[2])<<','<<Snapshot(light)<<']';
            }
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"factory","copy","clone","directional","point","spot","unknown","world"})(void)Run(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": DXLight reconstruction\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
