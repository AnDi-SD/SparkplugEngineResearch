#pragma once

// Inferred declaration path. The native class is the concrete standard leaf
// of spMaterialTextureLayer and constructs a plain spMaterialTexture payload.

#include "spMaterialTextureLayer.h"

namespace sparkplug::reconstruction
{
    class spStdLayer final : public spMaterialTextureLayer
    {
    public:
        static constexpr spClassID ClassID = 0x234C576B;

        spStdLayer();
        ~spStdLayer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
