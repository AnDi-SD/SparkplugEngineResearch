#pragma once

// Inferred declaration path. Native RTTI and the single relationship member
// are shared by PC and PS2; factory bodies differ by platform protection.

#include "../SparkBase/spBaseObject.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spMaterialTexture;

    class spMaterialTextureLayer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x7F577C6D;

        spMaterialTextureLayer() noexcept = default;
        ~spMaterialTextureLayer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] const std::shared_ptr<spMaterialTexture>&
            GetMaterialTextureForAnalysis() const noexcept;
        void SetMaterialTextureForAnalysis(
            std::shared_ptr<spMaterialTexture> texture) noexcept;

    private:
        std::shared_ptr<spMaterialTexture> materialTexture_;
    };
}
