#pragma once

#include "../Sparkplug/spRenderTargetManager.h"

namespace sparkplug::reconstruction
{
    class spPS2RenderTargetManager final : public spRenderTargetManager
    {
    public:
        static constexpr spClassID ClassID = 0x01486E51;

        spPS2RenderTargetManager() noexcept = default;
        ~spPS2RenderTargetManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
