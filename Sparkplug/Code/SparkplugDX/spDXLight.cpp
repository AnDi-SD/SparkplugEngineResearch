#include "spDXLight.h"
#include "Analysis/PC/spColorMath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXLight>();}
        const spRTTIRecord Record{spDXLight::ClassID,spLight::ClassID,"spDXLight",&spLight::StaticRTTI(),Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    }
    const spRTTIRecord& spDXLight::StaticRTTI()noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXLight::vfunc_18()const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spDXLight::vfunc_10(spCloneManager& manager)const
    {auto clone=std::make_unique<spDXLight>();manager.RegisterClone(*this,*clone);return vfunc_14(*clone,manager)?std::move(clone):nullptr;}
    bool spDXLight::vfunc_14(spBaseObject& target,spCloneManager& manager)const
    {
        auto* destination=dynamic_cast<spDXLight*>(&target);
        if(!destination||!spLight::vfunc_14(target,manager))return false;
        destination->payload_=payload_;return true; // inherited copy intentionally omits intensity
    }
    bool spDXLight::CopyIntoForAnalysis(spDXLight& destination)const
    {spCloneManager manager;return vfunc_14(destination,manager);}
    bool spDXLight::UpdateWorldForAnalysis(std::uint32_t inherited,const Matrix3* camera)noexcept
    {
        const bool refresh=NeedsDeviceRefreshForAnalysis(GetFlagsForAnalysis(),inherited,true);
        if(!spLight::UpdateWorldForAnalysis(inherited,camera))return false;
        if(!refresh||!IsLightEnabledForAnalysis())return true;
        const auto& world=GetWorldOrientationForAnalysis();
        return RefreshDevicePayloadForAnalysis(GetWorldPositionForAnalysis(),{world[6],world[7],world[8]},worldDefaultVector_,worldAmbientARGB_);
    }
    bool spDXLight::NeedsDeviceRefreshForAnalysis(std::uint32_t stored,std::uint32_t inherited,bool enabled)noexcept
    {auto flags=stored|inherited;if(flags&1)flags|=8;return enabled&&(flags&8);}
    bool spDXLight::RefreshDevicePayloadForAnalysis(const Vector3& position,const Vector3& direction,
        const Vector3& defaultVector,std::uint32_t ambientARGB)noexcept
    {
        const auto kind=static_cast<std::uint32_t>(GetTypeForAnalysis());const float range=GetRangeForAnalysis(),intensity=GetIntensityForAnalysis();
        if(!std::isfinite(range)||!std::isfinite(intensity)||!std::isfinite(GetHotspotAngleForAnalysis())||!std::isfinite(GetFalloffAngleForAnalysis()))return false;
        for(const auto* v:{&position,&direction,&defaultVector})for(float f:*v)if(!std::isfinite(f))return false;
        auto color=GetColorForAnalysis();for(float f:color)if(!std::isfinite(f))return false;
        const auto write=[this](unsigned index,float value){payload_[index]=Bits(value);};
        write(19,range);write(20,1);
        if(kind>2)return true; // native ambient/unknown type changes only these two words
        payload_[0]=kind==0?3:kind;
        const auto& pos=kind==0?defaultVector:position;const auto& dir=kind==1?defaultVector:direction;
        for(unsigned i=0;i<3;++i){write(13+i,pos[i]);write(16+i,dir[i]);}
        if(kind==0)
        {
            for(unsigned i=0;i<4;++i)color[i]=float(double(color[i])*double(intensity));
            for(unsigned i=0;i<3;++i)if(color[i]>1)color[i]=1; // no lower clamp, alpha not clamped
        }
        const auto ambient=PCARGBToRGBAForAnalysis(ambientARGB);
        for(unsigned i=0;i<4;++i){write(1+i,color[i]);write(5+i,color[i]);write(9+i,ambient[i]);}
        const bool attenuation=UsesAttenuationForAnalysis()&&range>0&&intensity>0;
        if(kind==0){write(21,1);write(22,0);write(23,0);return true;} // theta/phi remain untouched
        write(21,kind==1||attenuation?float(1.0/double(intensity)):1.F);
        write(22,attenuation?float(double(0.7F)/((double(intensity)*double(range))*double(0.3F))):0.F);
        write(23,0);write(24,kind==2?GetHotspotAngleForAnalysis():0.F);write(25,kind==2?GetFalloffAngleForAnalysis():0.F);
        return true;
    }
}
