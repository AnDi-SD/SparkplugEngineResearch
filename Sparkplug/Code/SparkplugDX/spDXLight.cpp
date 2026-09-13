#include "spDXLight.h"
#include "spPCLightPayload.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXLight>();}
        const spRTTIRecord Record{spDXLight::ClassID,spLight::ClassID,"spDXLight",&spLight::StaticRTTI(),Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
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
        PCLightPayloadInputsForAnalysis input{};
        input.kind=static_cast<std::uint32_t>(GetTypeForAnalysis());input.color=GetColorForAnalysis();
        input.position=position;input.direction=direction;input.defaultVector=defaultVector;
        input.range=GetRangeForAnalysis();input.intensity=GetIntensityForAnalysis();
        input.hotspot=GetHotspotAngleForAnalysis();input.falloff=GetFalloffAngleForAnalysis();
        input.ambientARGB=ambientARGB;input.attenuationEnabled=UsesAttenuationForAnalysis();
        return RefreshPCLightPayloadForAnalysis(input,payload_);
    }
}
