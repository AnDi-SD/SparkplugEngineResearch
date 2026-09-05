#pragma once

#include "../SparkplugDX/spDXRenderTarget.h"

namespace sparkplug::reconstruction
{
    class spPCRenderTarget final : public spDXRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x0C681FC8;

        spPCRenderTarget() noexcept = default;
        ~spPCRenderTarget() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
