#pragma once

// Inferred declaration path.  Both executables prove the type and hierarchy;
// no original spResource header/source path survives in their strings.

#include "../SparkBase/spBaseObject.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spResource : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x46F043FE;

        spResource() noexcept = default;
        ~spResource() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
