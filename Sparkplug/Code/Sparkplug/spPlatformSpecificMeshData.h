#pragma once

// Inferred declaration path. Both shipped executables prove the class and
// hierarchy, but neither retains an original header/source path for it.

#include "../SparkBase/spBaseObject.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spPlatformSpecificMeshData : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x71BE79C5;

        spPlatformSpecificMeshData() noexcept = default;
        ~spPlatformSpecificMeshData() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
