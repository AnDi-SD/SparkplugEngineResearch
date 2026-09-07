#pragma once

// Dependency slice. PC callers identify the sole extra base slot +0x1C
// as an update taking one float; original method name remains unknown.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spSubController : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0x062C22ED;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        virtual void ApplyForAnalysis(float time) = 0;
    };
} // namespace sparkplug::reconstruction
