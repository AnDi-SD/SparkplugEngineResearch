#pragma once

// Inferred source/header placement. Original class identity is preserved;
// this is a portable dependency prefix, not a complete native interface.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spEvaluator : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0xE91D088D;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

      protected:
        spEvaluator() noexcept = default;
    };
} // namespace sparkplug::reconstruction
