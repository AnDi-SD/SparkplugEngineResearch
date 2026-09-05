#pragma once

// Inferred declaration path. The exact serializer source path survives as
// Code/Sparkplug/spLightDataSerializer.cpp, not as this class translation unit.

#include "spLight.h"

namespace sparkplug::reconstruction
{
    class spLightData : public spLight
    {
    public:
        static constexpr spClassID ClassID = 0x5E6402DF;

        spLightData() noexcept;
        ~spLightData() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
