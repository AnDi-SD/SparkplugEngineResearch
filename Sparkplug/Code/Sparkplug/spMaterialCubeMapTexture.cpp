#include "spMaterialCubeMapTexture.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialCubeMapTexture()
        {
            return std::make_unique<spMaterialCubeMapTexture>();
        }

        const spRTTIRecord MaterialCubeMapTextureRecord{
            spMaterialCubeMapTexture::ClassID,
            spMaterialRenderTargetTexture::ClassID,
            "spMaterialCubeMapTexture",
            &spMaterialRenderTargetTexture::StaticRTTI(),
            &CreateMaterialCubeMapTexture,
            nullptr,
        };

        const bool MaterialCubeMapTextureRegistered =
            spRTTIManager::Instance().Register(MaterialCubeMapTextureRecord);
    }

    const spRTTIRecord& spMaterialCubeMapTexture::StaticRTTI() noexcept
    {
        (void)MaterialCubeMapTextureRegistered;
        return MaterialCubeMapTextureRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialCubeMapTexture::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialCubeMapTexture>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialCubeMapTexture::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target =
            dynamic_cast<spMaterialCubeMapTexture*>(&destination);
        if (target == nullptr
            || !spMaterialRenderTargetTexture::vfunc_14(destination, manager))
        {
            return false;
        }
        target->sourceRenderNode_ = sourceRenderNode_;
        target->ownedCamera_ = ownedCamera_;
        target->facesPerTick_ = facesPerTick_;
        target->currentFace_ = 0;
        return true;
    }

    const spRTTIRecord& spMaterialCubeMapTexture::vfunc_18() const noexcept
    {
        return MaterialCubeMapTextureRecord;
    }

    spMaterialRenderTargetTexture::RenderKind
    spMaterialCubeMapTexture::GetRenderKindForAnalysis() const noexcept
    {
        return RenderKind::CubeMap;
    }

    std::unique_ptr<spMaterialRenderTargetTexture>
    spMaterialCubeMapTexture::CopyRenderTargetTextureForAnalysis() const
    {
        auto cloneBase = Clone();
        auto* const raw =
            dynamic_cast<spMaterialCubeMapTexture*>(cloneBase.release());
        return std::unique_ptr<spMaterialRenderTargetTexture>(raw);
    }

    spBaseObject* spMaterialCubeMapTexture::GetSourceRenderNodeForAnalysis()
        const noexcept
    {
        return sourceRenderNode_;
    }

    void spMaterialCubeMapTexture::SetSourceRenderNodeForAnalysis(
        spBaseObject* source) noexcept
    {
        sourceRenderNode_ = source;
    }

    spBaseObject* spMaterialCubeMapTexture::GetOwnedCameraForAnalysis()
        const noexcept
    {
        return ownedCamera_;
    }

    void spMaterialCubeMapTexture::SetOwnedCameraForAnalysis(
        spBaseObject* camera) noexcept
    {
        ownedCamera_ = camera;
    }

    std::uint32_t
    spMaterialCubeMapTexture::GetNumFacesToRenderPerTickForAnalysis()
        const noexcept
    {
        return facesPerTick_;
    }

    void spMaterialCubeMapTexture::SetNumFacesToRenderPerTickForAnalysis(
        const std::uint32_t count) noexcept
    {
        facesPerTick_ = std::clamp(count, std::uint32_t{1}, CubeFaceCount);
    }

    std::uint32_t spMaterialCubeMapTexture::GetCurrentFaceForAnalysis()
        const noexcept
    {
        return currentFace_;
    }

    void spMaterialCubeMapTexture::AdvanceFacesForAnalysis() noexcept
    {
        currentFace_ = (currentFace_ + facesPerTick_) % CubeFaceCount;
    }
}
