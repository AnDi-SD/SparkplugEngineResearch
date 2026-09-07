#pragma once

// Original class/RTTI and PC behavior; file path and API names inferred.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spTrack : public spNamedObject
    {
      public:
        static constexpr spClassID ClassID = 0x60C839C5;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        // Original PC extra slots +0x1c/+0x20. Concrete, not pure virtual.
        [[nodiscard]] virtual float GetDurationForAnalysis() const noexcept;
        virtual void ReleaseKeysForAnalysis() noexcept;
    };
} // namespace sparkplug::reconstruction
