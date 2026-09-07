#include "spLightManager.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightManager()
        {
            return std::make_unique<spLightManager>();
        }
        const spRTTIRecord LightManagerRecord{spLightManager::ClassID, spBaseObject::ClassID,
                                              "spLightManager",        &spBaseObject::StaticRTTI(),
                                              &CreateLightManager,     nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(LightManagerRecord);
    } // namespace

    const spRTTIRecord& spLightManager::StaticRTTI() noexcept
    {
        (void)Registered;
        return LightManagerRecord;
    }
    const spRTTIRecord& spLightManager::vfunc_18() const noexcept
    {
        return LightManagerRecord;
    }

    std::unique_ptr<spBaseObject> spLightManager::vfunc_10(spCloneManager& manager) const
    {
        // Original46ABF0 creates a fresh manager and uses inherited base copy,
        // not a clone of borrowed lists or initialized scene-owner state.
        auto clone = std::make_unique<spLightManager>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spLightManager::RegisterLightForAnalysis(spLight& light)
    {
        if (lights_.size() >= 4096 ||
            std::find(lights_.begin(), lights_.end(), &light) != lights_.end())
            return false;
        lights_.push_back(&light);
        return true;
    }
    bool spLightManager::UnregisterLightForAnalysis(spLight& light) noexcept
    {
        const auto found = std::find(lights_.begin(), lights_.end(), &light);
        if (found == lights_.end())
            return false;
        lights_.erase(found); // stable list order, unlike cache swap-last removal
        return true;
    }

    spLight* spLightManager::CacheForAnalysis::Get(const std::size_t index) const noexcept
    {
        return index < count_ ? lights_[index] : nullptr;
    }
    void spLightManager::CacheForAnalysis::ResetSelection() noexcept
    {
        ambient_ = nullptr;
        count_ = 0;
    }
    void spLightManager::CacheForAnalysis::Add(spLight& light) noexcept
    {
        if (light.GetTypeForAnalysis() == spLight::Type::Ambient)
        {
            if (!ambient_)
                ambient_ = &light;
            return;
        }
        if (count_ == OrdinaryLightCapacity)
            return;
        for (std::size_t i = 0; i < count_; ++i)
            if (lights_[i] == &light)
                return;
        lights_[count_++] = &light;
    }
    void spLightManager::CacheForAnalysis::Remove(spLight& light) noexcept
    {
        if (light.GetTypeForAnalysis() == spLight::Type::Ambient)
        {
            if (ambient_ == &light)
                ambient_ = nullptr;
            return;
        }
        for (std::size_t i = 0; i < count_; ++i)
        {
            if (lights_[i] != &light)
                continue;
            --count_;
            if (i == count_)
                lights_[i] = nullptr;
            else
                lights_[i] = lights_[count_]; // native leaves unused tail unchanged
            return;
        }
    }

    bool spLightManager::IsEligibleForAnalysis(const spLight& light,
                                               const SphereForAnalysis& worldSphere,
                                               const bool excludeShadowVolumeLights) noexcept
    {
        if (!light.IsLightEnabledForAnalysis() || !light.IsHierarchyActiveForAnalysis() ||
            (excludeShadowVolumeLights && light.ProjectsShadowVolumeForAnalysis()))
            return false;
        const auto type = light.GetTypeForAnalysis();
        if (type == spLight::Type::Directional || type == spLight::Type::Ambient)
            return true;
        const auto& position = light.GetWorldPositionForAnalysis();
        const float x = position[0] - worldSphere[0], y = position[1] - worldSphere[1],
                    z = position[2] - worldSphere[2];
        const float radius = light.GetRangeForAnalysis() + worldSphere[3];
        return x * x + y * y + z * z <= radius * radius;
    }
    void spLightManager::RebuildCacheForAnalysis(
        CacheForAnalysis& cache, const SphereForAnalysis& worldSphere,
        const bool excludeShadowVolumeLights) const noexcept
    {
        cache.ResetSelection();
        for (auto* light : lights_)
            if (IsEligibleForAnalysis(*light, worldSphere, excludeShadowVolumeLights))
                cache.Add(*light);
    }
    void spLightManager::RefreshLightForAnalysis(CacheForAnalysis& cache, spLight& light,
                                                 const SphereForAnalysis& worldSphere,
                                                 const bool excludeShadowVolumeLights) noexcept
    {
        if (IsEligibleForAnalysis(light, worldSphere, excludeShadowVolumeLights))
            cache.Add(light);
        else
            cache.Remove(light);
    }

    bool spLightManager::RegisterRenderTargetForAnalysis(CacheForAnalysis& cache,
        const SphereForAnalysis& sphere,const bool excludeShadowVolumeLights)
    {
        if(renderTargets_.size()>=4096||std::any_of(renderTargets_.begin(),renderTargets_.end(),
            [&](const auto& target){return target.cache==&cache;}))return false;
        renderTargets_.push_back({&cache,&sphere,excludeShadowVolumeLights});return true;
    }
    bool spLightManager::UnregisterRenderTargetForAnalysis(CacheForAnalysis& cache) noexcept
    {
        const auto found=std::find_if(renderTargets_.begin(),renderTargets_.end(),
            [&](const auto& target){return target.cache==&cache;});
        if(found==renderTargets_.end())return false;
        renderTargets_.erase(found);return true;
    }
    void spLightManager::RefreshRenderTargetsForAnalysis(spLight& light) noexcept
    {
        // PC46ACE0/46AC60 preserves target order and reads current world spheres.
        for(const auto& target:renderTargets_)
            RefreshLightForAnalysis(*target.cache,light,*target.sphere,target.excludeShadowVolumeLights);
    }
} // namespace sparkplug::reconstruction
