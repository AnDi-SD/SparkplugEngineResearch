#include "spMaterialCameraViewTexture.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialCameraViewTexture()
        {
            return std::make_unique<spMaterialCameraViewTexture>();
        }

        const spRTTIRecord MaterialCameraViewTextureRecord{
            spMaterialCameraViewTexture::ClassID,
            spMaterialRenderTargetTexture::ClassID,
            "spMaterialCameraViewTexture",
            &spMaterialRenderTargetTexture::StaticRTTI(),
            &CreateMaterialCameraViewTexture,
            nullptr,
        };

        const bool MaterialCameraViewTextureRegistered =
            spRTTIManager::Instance().Register(MaterialCameraViewTextureRecord);
    }

    const spRTTIRecord& spMaterialCameraViewTexture::StaticRTTI() noexcept
    {
        (void)MaterialCameraViewTextureRegistered;
        return MaterialCameraViewTextureRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialCameraViewTexture::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialCameraViewTexture>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialCameraViewTexture::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target =
            dynamic_cast<spMaterialCameraViewTexture*>(&destination);
        if (target == nullptr
            || !spMaterialRenderTargetTexture::vfunc_14(destination, manager))
        {
            return false;
        }
        target->cameraName_ = cameraName_;
        target->resolvedCamera_ = resolvedCamera_;
        return true;
    }

    const spRTTIRecord& spMaterialCameraViewTexture::vfunc_18()
        const noexcept
    {
        return MaterialCameraViewTextureRecord;
    }

    spMaterialRenderTargetTexture::RenderKind
    spMaterialCameraViewTexture::GetRenderKindForAnalysis() const noexcept
    {
        return RenderKind::CameraView;
    }

    std::unique_ptr<spMaterialRenderTargetTexture>
    spMaterialCameraViewTexture::CopyRenderTargetTextureForAnalysis() const
    {
        auto cloneBase = Clone();
        auto* const raw =
            dynamic_cast<spMaterialCameraViewTexture*>(cloneBase.release());
        return std::unique_ptr<spMaterialRenderTargetTexture>(raw);
    }

    const char* spMaterialCameraViewTexture::GetCameraNameForAnalysis()
        const noexcept
    {
        return cameraName_.empty() ? nullptr : cameraName_.c_str();
    }

    void spMaterialCameraViewTexture::SetCameraNameForAnalysis(
        const std::string_view name)
    {
        cameraName_.assign(name.data(), name.size());
    }

    spBaseObject* spMaterialCameraViewTexture::GetResolvedCameraForAnalysis()
        const noexcept
    {
        return resolvedCamera_;
    }

    void spMaterialCameraViewTexture::SetResolvedCameraForAnalysis(
        spBaseObject* camera) noexcept
    {
        resolvedCamera_ = camera;
    }
}
