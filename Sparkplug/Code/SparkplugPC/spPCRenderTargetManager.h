#pragma once

#include "../Sparkplug/spRenderTargetManager.h"

namespace sparkplug::reconstruction
{
    class spPCRenderTargetManager final : public spRenderTargetManager
    {
    public:
        static constexpr spClassID ClassID = 0x165C006F;

        spPCRenderTargetManager() noexcept = default;
        ~spPCRenderTargetManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
