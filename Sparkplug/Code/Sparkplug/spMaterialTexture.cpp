#include "spMaterialTexture.h"

#include "spTexture.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialTexture()
        {
            return std::make_unique<spMaterialTexture>();
        }

        const spRTTIRecord MaterialTextureRecord{
            spMaterialTexture::ClassID,
            spBaseObject::ClassID,
            "spMaterialTexture",
            &spBaseObject::StaticRTTI(),
            &CreateMaterialTexture,
            nullptr,
        };

        const bool MaterialTextureRegistered =
            spRTTIManager::Instance().Register(MaterialTextureRecord);
    }

    spMaterialTexture::spMaterialTexture() noexcept
    {
        uvTransform_[0] = 1.0F;
        uvTransform_[4] = 1.0F;
        uvTransform_[8] = 1.0F;
    }

    spMaterialTexture::~spMaterialTexture() = default;

    const spRTTIRecord& spMaterialTexture::StaticRTTI() noexcept
    {
        (void)MaterialTextureRegistered;
        return MaterialTextureRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialTexture::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialTexture>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialTexture::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target = dynamic_cast<spMaterialTexture*>(&destination);
        if (target == nullptr || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }
        target->textureStates_ = textureStates_;
        target->fallbackTexture_ = fallbackTexture_;
        target->animationController_ = animationController_;
        target->uvTransform_ = uvTransform_;
        target->hasStaticUV_ = hasStaticUV_;
        target->uvController_ = uvController_;
        return true;
    }

    const spRTTIRecord& spMaterialTexture::vfunc_18() const noexcept
    {
        return MaterialTextureRecord;
    }

    spTexture* spMaterialTexture::GetTextureForAnalysis() const noexcept
    {
        return fallbackTexture_;
    }

    spTexture* spMaterialTexture::GetFallBackTextureForAnalysis() const noexcept
    {
        return fallbackTexture_;
    }

    spBaseObject* spMaterialTexture::GetAnimTextureControllerForAnalysis()
        const noexcept
    {
        return animationController_;
    }

    spBaseObject* spMaterialTexture::GetUVControllerForAnalysis() const noexcept
    {
        return uvController_;
    }

    const std::array<std::uint32_t, spMaterialTexture::PS2TextureStateCount>&
    spMaterialTexture::GetTextureStatesForAnalysis() const noexcept
    {
        return textureStates_;
    }

    const std::array<float, spMaterialTexture::UVTransformValueCount>&
    spMaterialTexture::GetUVTransformForAnalysis() const noexcept
    {
        return uvTransform_;
    }

    bool spMaterialTexture::HasStaticTransformForAnalysis() const noexcept
    {
        return hasStaticUV_;
    }

    void spMaterialTexture::SetFallBackTextureForAnalysis(
        spTexture* texture) noexcept
    {
        fallbackTexture_ = texture;
    }

    void spMaterialTexture::SetAnimTextureControllerForAnalysis(
        spBaseObject* controller) noexcept
    {
        animationController_ = controller;
    }

    void spMaterialTexture::SetUVControllerForAnalysis(
        spBaseObject* controller) noexcept
    {
        uvController_ = controller;
    }

    void spMaterialTexture::SetTextureStateForAnalysis(
        const std::size_t index,
        const std::uint32_t value) noexcept
    {
        if (index < textureStates_.size())
        {
            textureStates_[index] = value;
        }
    }

    void spMaterialTexture::SetStaticUVTransformForAnalysis(
        const std::array<float, UVTransformValueCount>& transform) noexcept
    {
        uvTransform_ = transform;
        hasStaticUV_ = true;
    }

    void spMaterialTexture::ClearStaticUVTransformForAnalysis() noexcept
    {
        hasStaticUV_ = false;
    }
}
