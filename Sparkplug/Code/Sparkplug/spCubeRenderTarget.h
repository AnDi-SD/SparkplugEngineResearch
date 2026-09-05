#pragma once

#include "spRenderTarget.h"

namespace sparkplug::reconstruction
{
    class spCubeRenderTarget : public spRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x0F8B095F;
        static constexpr std::size_t FaceCount = 6;

        ~spCubeRenderTarget() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

    protected:
        using spRenderTarget::spRenderTarget;
    };
}
