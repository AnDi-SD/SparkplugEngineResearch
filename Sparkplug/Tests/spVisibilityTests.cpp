#include "Code/Sparkplug/spVisibilityManager.h"
#include "Analysis/PC/SparkplugAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace vm = sparkplug::evidence::pc::visibility_math;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    float ReadFloat()
    {
        std::uint32_t bits;
        if (!(std::cin >> bits))
            throw std::runtime_error("incomplete float bits");
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    vm::PlaneSet ReadPlanes(std::uint32_t count, std::uint32_t active)
    {
        if (count > 16)
            throw std::runtime_error("bounded planes");
        vm::PlaneSet result;
        result.activeCount = active;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            vm::Plane plane;
            for (auto& value : plane.equation)
                value = ReadFloat();
            unsigned enabled;
            std::cin >> enabled;
            plane.enabled = enabled != 0;
            result.planes.push_back(plane);
        }
        return result;
    }
    void MathBatch()
    {
        std::uint32_t count, active;
        while (std::cin >> count >> active)
        {
            vm::Sphere sphere;
            for (auto& value : sphere)
                value = ReadFloat();
            auto planes = ReadPlanes(count, active);
            std::cout << vm::IsOutside(planes, sphere) << ','
                      << static_cast<unsigned>(vm::Classify(planes, sphere)) << ','
                      << vm::IsFullyInside(planes, sphere) << '\n';
        }
    }
    void SelectBatch()
    {
        using Manager = spVisibilityManager;
        std::uint32_t seed, culling, debug;
        while (std::cin >> seed >> culling >> debug)
        {
            auto planes = ReadPlanes(6, 6);
            Manager manager;
            manager.SetFrameStampForAnalysis(seed);
            manager.SetSphereCullingForAnalysis(culling != 0);
            std::array<Manager::SupportForAnalysis, 6> objects;
            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                auto& obj = objects[i];
                obj.kind = i == 0  ? Manager::SupportKindForAnalysis::Partition
                           : i < 3 ? Manager::SupportKindForAnalysis::Static
                                   : Manager::SupportKindForAnalysis::RenderNode;
                unsigned enabled, bypass;
                std::cin >> obj.visibilityMark >> enabled >> bypass;
                obj.enabled = enabled != 0;
                obj.cullBypass = bypass != 0;
                for (auto& value : obj.worldSphere)
                    value = ReadFloat();
            }
            std::uint32_t sceneStamp;
            manager.BeginFrameForAnalysis(sceneStamp);
            // Explicit original list order with duplicate static/dynamic refs.
            for (auto i : {0, 1, 2, 1, 3, 4, 5, 3})
                (void)manager.TrySubmitForAnalysis(objects[i], planes, debug != 0);
            std::cout << sceneStamp << ',' << manager.GetVisibleForAnalysis().size();
            for (const auto* object : manager.GetVisibleForAnalysis())
                std::cout << ',' << object - objects.data();
            for (const auto& object : objects)
                std::cout << ',' << object.visibilityMark;
            std::cout << '\n';
        }
    }
    void UnitTests()
    {
        vm::PlaneSet planes{{{{{0, 0, 1, 0}}, true}}, 1};
        Check(!vm::IsOutside(planes, {0, 0, -1, 1}), "negative tangent retained");
        Check(vm::IsOutside(planes, {0, 0, -2, 1}), "strict outside");
        Check(vm::Classify(planes, {0, 0, 2, 1}) == vm::SphereClass::Inside, "inside");
        Check(vm::Classify(planes, {0, 0, 1, 1}) == vm::SphereClass::Intersecting,
              "positive tangent");
        Check(vm::Classify(planes, {0, 0, -1, 1}) == vm::SphereClass::Intersecting,
              "negative tangent");
        planes.activeCount = 0;
        Check(!vm::IsOutside(planes, {0, 0, -2, 1}), "cached-count gate");
        Check(vm::Classify(planes, {0, 0, -2, 1}) == vm::SphereClass::Outside,
              "classifier ignores count");
        planes.activeCount = 1;
        const vm::Sphere nan{0, 0, std::numeric_limits<float>::quiet_NaN(), 1};
        Check(!vm::IsOutside(planes, nan), "unordered not outside");
        Check(vm::Classify(planes, nan) == vm::SphereClass::Intersecting, "unordered intersecting");
        Check(vm::IsFullyInside(planes, {0, 0, 2, 1}), "strictly fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, 1, 1}), "positive tangent not fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, -1, 1}), "negative tangent not fully inside");
        Check(!vm::IsFullyInside(planes, nan), "unordered not fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, 0, -1}),
              "negative radius first rejection retained");
        planes.activeCount = 0;
        Check(vm::IsFullyInside(planes, {0, 0, -2, 1}), "empty cached count fully inside");
        planes.activeCount = 1;
        planes.planes[0].enabled = false;
        Check(vm::IsFullyInside(planes, {0, 0, -2, 1}),
              "disabled plane ignored for full inclusion");
        planes.planes[0].enabled = true;
        spVisibilityManager manager;
        spVisibilityManager::SupportForAnalysis object;
        object.worldSphere = {0, 0, 2, 1};
        std::uint32_t sceneStamp = 42;
        manager.BeginFrameForAnalysis(sceneStamp);
        Check(sceneStamp == 1 && manager.GetFrameStampForAnalysis() == 1, "frame publish");
        Check(manager.TrySubmitForAnalysis(object, planes), "submit");
        Check(!manager.TrySubmitForAnalysis(object, planes), "deduplicate");
        manager.SetFrameStampForAnalysis(0xffffffffu);
        object.visibilityMark = 0;
        manager.BeginFrameForAnalysis(sceneStamp);
        Check(sceneStamp == 0 && !manager.TrySubmitForAnalysis(object, planes), "native zero wrap");
        manager.BeginFrameForAnalysis(sceneStamp);
        object.kind = spVisibilityManager::SupportKindForAnalysis::RenderNode;
        object.enabled = false;
        Check(!manager.TrySubmitForAnalysis(object, planes), "disabled dynamic");
        Check(manager.TrySubmitForAnalysis(object, planes, true), "debug bypasses disabled");
        auto clone = manager.Clone();
        auto* typed = dynamic_cast<spVisibilityManager*>(clone.get());
        Check(typed && typed->GetFrameStampForAnalysis() == 0 &&
                  typed->GetVisibleForAnalysis().empty(),
              "inherited-only clone fresh own state");
        Check(manager.IsExactly(spVisibilityManager::ClassID) &&
                  manager.IsKindOf(spBaseObject::ClassID),
              "original RTTI identity");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--math-batch")
            MathBatch();
        else if (argc == 2 && std::string(argv[1]) == "--select-batch")
            SelectBatch();
        else
        {
            UnitTests();
            std::cout << "PASS " << checks << '/' << checks << ": visibility slice\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
