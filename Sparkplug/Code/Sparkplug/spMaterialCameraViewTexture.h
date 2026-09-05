#pragma once

#include "spMaterialRenderTargetTexture.h"

#include <string>
#include <string_view>

namespace sparkplug::reconstruction
{
    class spMaterialCameraViewTexture final
        : public spMaterialRenderTargetTexture
    {
    public:
        static constexpr spClassID ClassID = 0x34EF51B9;

        spMaterialCameraViewTexture() = default;
        ~spMaterialCameraViewTexture() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] RenderKind GetRenderKindForAnalysis()
            const noexcept override;
        [[nodiscard]] std::unique_ptr<spMaterialRenderTargetTexture>
            CopyRenderTargetTextureForAnalysis() const override;

        [[nodiscard]] const char* GetCameraNameForAnalysis() const noexcept;
        void SetCameraNameForAnalysis(std::string_view name);
        [[nodiscard]] spBaseObject* GetResolvedCameraForAnalysis() const noexcept;
        void SetResolvedCameraForAnalysis(spBaseObject* camera) noexcept;

    private:
        std::string cameraName_;
        spBaseObject* resolvedCamera_ = nullptr;
    };
}
