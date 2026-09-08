#include "spParticleSystem.h"
#include "Analysis/PC/spParticleSampling.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateParticleSystem(){return std::make_unique<spParticleSystem>();}
        const spRTTIRecord Record{spParticleSystem::ClassID,spRenderable::ClassID,"spParticleSystem",
            &spRenderable::StaticRTTI(),&CreateParticleSystem,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spParticleSystem::StaticRTTI() noexcept {(void)Registered;return Record;}
    const spRTTIRecord& spParticleSystem::vfunc_18() const noexcept {return Record;}
    std::unique_ptr<spBaseObject> spParticleSystem::vfunc_10(spCloneManager&) const {return nullptr;}
    bool spParticleSystem::vfunc_14(spBaseObject&,spCloneManager&) const {return false;}
    bool spParticleSystem::SampleEmissionRegionForAnalysis(sparkplug::evidence::pc::ParticleRandomForAnalysis& random,
        std::uint32_t count,std::vector<Vector3>& output) const
    {return sparkplug::evidence::pc::SampleParticleRegionForAnalysis(parameters_.regionType,parameters_.region,random,count,output);}
    bool spParticleSystem::PrepareNonLoopingForAnalysis() noexcept
    {
        if(parameters_.flags[0])return false;
        const double duration=parameters_.times[0]<0?parameters_.times[1]:parameters_.times[0];
        const double count=duration*double(parameters_.rate);
        // Native truncates then clamps unsigned count to65536. Its zero-count
        // list initialization is unsafe. These are explicit portable bounds.
        if(!std::isfinite(count)||count<1||count>=1025)return false;
        poolState_={0,static_cast<std::uint32_t>(count),0,0,0};
        return true;
    }
}
