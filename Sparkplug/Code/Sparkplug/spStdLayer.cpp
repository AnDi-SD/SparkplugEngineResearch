#include "spStdLayer.h"

#include "spMaterialTexture.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateStdLayer()
        {
            return std::make_unique<spStdLayer>();
        }

        const spRTTIRecord StdLayerRecord{
            spStdLayer::ClassID,
            spMaterialTextureLayer::ClassID,
            "spStdLayer",
            &spMaterialTextureLayer::StaticRTTI(),
            &CreateStdLayer,
            nullptr,
        };

        const bool StdLayerRegistered =
            spRTTIManager::Instance().Register(StdLayerRecord);
    }

    spStdLayer::spStdLayer()
    {
        SetMaterialTextureForAnalysis(std::make_shared<spMaterialTexture>());
    }

    spStdLayer::~spStdLayer() = default;

    const spRTTIRecord& spStdLayer::StaticRTTI() noexcept
    {
        (void)StdLayerRegistered;
        return StdLayerRecord;
    }

    std::unique_ptr<spBaseObject> spStdLayer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spStdLayer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spStdLayer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spMaterialTextureLayer::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spStdLayer::vfunc_18() const noexcept
    {
        return StdLayerRecord;
    }
}
