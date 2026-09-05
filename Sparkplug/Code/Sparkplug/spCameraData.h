#pragma once

#include "spCamera.h"

namespace sparkplug::reconstruction
{
    class spCameraData final : public spCamera
    {
    public:
        static constexpr spClassID ClassID = 0x24BB4C41;

        spCameraData() noexcept = default;
        ~spCameraData() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
