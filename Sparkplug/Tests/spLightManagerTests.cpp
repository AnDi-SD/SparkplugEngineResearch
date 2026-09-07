#include "Code/Sparkplug/spLightManager.h"
#include "Code/Sparkplug/spLightData.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* text)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(text);
    }
    void Units()
    {
        spLightManager manager;
        std::array<spLightData, 11> lights;
        Check(manager.IsExactly(spLightManager::ClassID) && manager.IsKindOf(spBaseObject::ClassID),
              "original light manager RTTI/base");
        for (auto& light : lights)
        {
            light.SetHierarchyActiveForAnalysis(true);
            Check(manager.RegisterLightForAnalysis(light), "non-owning ordered registration");
        }
        Check(!manager.RegisterLightForAnalysis(lights[0]) &&
                  manager.GetLightsForAnalysis().size() == 11,
              "host duplicate guard does not duplicate native list membership");
        lights[9].SetTypeForAnalysis(spLight::Type::Ambient);
        lights[10].SetTypeForAnalysis(spLight::Type::Ambient);
        spLightManager::CacheForAnalysis cache;
        manager.RebuildCacheForAnalysis(cache, {0, 0, 0, 2});
        Check(cache.GetCount() == 8 && cache.GetAmbient() == &lights[9],
              "eight ordinary/first ambient selection");
        Check(cache.Get(7) == &lights[7] && cache.Get(8) == nullptr,
              "ordered bounded ordinary selection");
        cache.Remove(lights[1]);
        Check(cache.Get(1) == &lights[7] && cache.GetRawSlots()[7] == &lights[7] &&
                  cache.GetCount() == 7,
              "middle cache removal preserves stale unused tail");
        cache.Add(lights[8]);
        cache.Remove(lights[8]);
        Check(cache.GetRawSlots()[7] == nullptr && cache.GetCount() == 7,
              "last cache removal clears tail");
        cache.Remove(lights[10]);
        Check(cache.GetAmbient() == &lights[9], "unselected ambient removal no-op");
        cache.ResetSelection();
        Check(cache.GetCount() == 0 && !cache.GetAmbient() && cache.GetRawSlots()[0] == &lights[0],
              "rebuild reset clears selection not unused raw pointers");
        auto& point = lights[0];
        point.SetTypeForAnalysis(spLight::Type::Point);
        point.SetRangeForAnalysis(3);
        point.SetPositionForAnalysis({5, 0, 0});
        Check(point.UpdateWorldForAnalysis(1), "seed point-light world position");
        point.SetEnabledForAnalysis(false);
        Check(spLightManager::IsEligibleForAnalysis(point, {0, 0, 0, 2}, true),
              "tangent accepted despite Node Enabled200 false");
        point.SetHierarchyActiveForAnalysis(false);
        Check(!spLightManager::IsEligibleForAnalysis(point, {0, 0, 0, 2}, true),
              "hierarchy100 required");
        point.SetHierarchyActiveForAnalysis(true);
        point.SetProjectsShadowVolumeForAnalysis(true);
        Check(!spLightManager::IsEligibleForAnalysis(point, {0, 0, 0, 2}, true) &&
                  spLightManager::IsEligibleForAnalysis(point, {0, 0, 0, 2}, false),
              "target-specific shadow exclusion");
        point.SetLightEnabledForAnalysis(false);
        Check(!spLightManager::IsEligibleForAnalysis(point, {0, 0, 0, 2}, false),
              "separate light enabled gate");
        Check(manager.UnregisterLightForAnalysis(lights[1]) &&
                  manager.GetLightsForAnalysis()[1] == &lights[2],
              "manager list unlink stable, unlike cache swap-last");
        auto clone = manager.Clone();
        auto* copied = dynamic_cast<spLightManager*>(clone.get());
        Check(copied && copied->GetLightsForAnalysis().empty(), "native blank manager clone");
        auto root = std::make_shared<spNode>();
        auto child = std::make_shared<spLightData>();
        Check(root->AttachChildForAnalysis(child), "hierarchy flag fixture");
        root->SetHierarchyActiveForAnalysis(true);
        Check(child->IsHierarchyActiveForAnalysis() && child->IsEnabledForAnalysis(),
              "bit100 recursive independently from200");
        root->SetHierarchyActiveForAnalysis(false);
        Check(!child->IsHierarchyActiveForAnalysis() && child->IsEnabledForAnalysis(),
              "clear100 preserves200");
    }
    int Batch()
    {
        std::array<spLightData, 11> lights;
        lights[9].SetTypeForAnalysis(spLight::Type::Ambient);
        lights[10].SetTypeForAnalysis(spLight::Type::Ambient);
        spLightManager::CacheForAnalysis cache;
        char operation;
        while (std::cin >> operation)
        {
            if (operation == 'E')
            {
                unsigned type, flags, enabled, shadow, exclude;
                float range;
                spNode::Vector3 position{};
                spLightManager::SphereForAnalysis sphere{};
                if (!(std::cin >> type >> flags >> enabled >> shadow >> exclude >> range))
                    return 2;
                for (auto& value : position)
                    std::cin >> value;
                for (auto& value : sphere)
                    std::cin >> value;
                auto& light = lights[0];
                light.SetTypeForAnalysis(static_cast<spLight::Type>(type));
                light.SetHierarchyActiveForAnalysis((flags & 0x100) != 0);
                light.SetEnabledForAnalysis((flags & 0x200) != 0);
                light.SetLightEnabledForAnalysis(enabled != 0);
                light.SetProjectsShadowVolumeForAnalysis(shadow != 0);
                light.SetRangeForAnalysis(range);
                light.SetPositionForAnalysis(position);
                if (!light.UpdateWorldForAnalysis(1))
                    return 3;
                std::cout << '['
                          << (spLightManager::IsEligibleForAnalysis(light, sphere, exclude != 0)
                                  ? 1
                                  : 0)
                          << "]\n";
                continue;
            }
            unsigned index = 0;
            if (operation != 'Z' && (!(std::cin >> index) || index >= lights.size()))
                return 4;
            if (operation == 'A')
                cache.Add(lights[index]);
            else if (operation == 'R')
                cache.Remove(lights[index]);
            else if (operation == 'Z')
                cache.ResetSelection();
            else
                return 5;
            auto id = [&](spLight* light) {
                for (std::size_t i = 0; i < lights.size(); ++i)
                    if (light == &lights[i])
                        return static_cast<int>(i);
                return -1;
            };
            std::cout << '[' << cache.GetCount() << ',' << id(cache.GetAmbient());
            for (auto* light : cache.GetRawSlots())
                std::cout << ',' << id(light);
            std::cout << "]\n";
        }
        return 0;
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            return Batch();
        Units();
        std::cout << "PASS " << checks << '/' << checks << " light-manager reconstruction checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
