#include "spLightData.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightData()
        {
            return std::make_unique<spLightData>();
        }

        const spRTTIRecord LightDataRecord{
            spLightData::ClassID,
            spLight::ClassID,
            "spLightData",
            &spLight::StaticRTTI(),
            &CreateLightData,
            nullptr,
        };

        const bool LightDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(LightDataRecord);
    }

    spLightData::spLightData() noexcept = default;

    spLightData::~spLightData() = default;

    const spRTTIRecord& spLightData::StaticRTTI() noexcept
    {
        (void)LightDataRegistered;
        return LightDataRecord;
    }

    std::unique_ptr<spBaseObject> spLightData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightData>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spLightData::vfunc_18() const noexcept
    {
        return LightDataRecord;
    }
}
