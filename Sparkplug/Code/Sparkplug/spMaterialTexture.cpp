#include "spMaterialTexture.h"

#include "spTexture.h"
#include "spAnimTexController.h"
#include "spUVController.h"

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialTextureRecord);
    }

    spMaterialTexture::spMaterialTexture() noexcept
    {
        // Confirmed PC68 factory defaults. Extra PS2-only slots remain outside
        // this PC proof; the portable holder retains their separate capacity.
        textureStates_[1]=3;textureStates_[2]=1;textureStates_[5]=0xFF000000;textureStates_[6]=2;
        uvTransform_[0] = 1.0F;
        uvTransform_[4] = 1.0F;
        uvTransform_[8] = 1.0F;
    }

    spMaterialTexture::~spMaterialTexture()
    {
        // Host context can outlive a holder; do not leave its canonical
        // controller pointing into destroyed storage. Native uses raw backlink.
        if(animationOwner_&&animationOwner_->GetMaterialForAnalysis()==this)
            animationOwner_->BindMaterialForAnalysis(nullptr);
        animationOwner_.reset();
        if(uvOwner_&&uvOwner_->GetMaterialForAnalysis()==this)uvOwner_->DetachMaterialForAnalysis();
        uvOwner_.reset();
    }

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
        if (target == nullptr || animationController_ || uvController_
            || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }
        target->textureStates_ = textureStates_;
        target->fallbackTexture_ = fallbackTexture_;
        target->fallbackOwner_ = fallbackOwner_;
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
        if(fallbackTexture_==texture)return;
        fallbackOwner_.reset();
        fallbackTexture_ = texture;
    }

    void spMaterialTexture::SetOwnedFallBackTextureForAnalysis(std::shared_ptr<spTexture> texture) noexcept
    {
        fallbackOwner_=std::move(texture);fallbackTexture_=fallbackOwner_.get();
    }

    void spMaterialTexture::SetAnimTextureControllerForAnalysis(
        spBaseObject* controller) noexcept
    {
        if(animationController_==controller)
        {if(animationOwner_)animationOwner_->BindMaterialForAnalysis(this);return;}
        if(animationOwner_&&animationOwner_->GetMaterialForAnalysis()==this)
            animationOwner_->BindMaterialForAnalysis(nullptr);
        animationOwner_.reset(); // raw overload remains explicitly borrowed
        animationController_ = controller;
    }

    void spMaterialTexture::SetOwnedAnimTextureControllerForAnalysis(std::shared_ptr<spAnimTexController> controller) noexcept
    {
        if(animationOwner_!=controller&&animationOwner_&&animationOwner_->GetMaterialForAnalysis()==this)
            animationOwner_->BindMaterialForAnalysis(nullptr);
        animationOwner_=std::move(controller);animationController_=animationOwner_.get();
        // Original476680 NULL dereferences; host explicitly allows safe clear.
        if(animationOwner_)animationOwner_->BindMaterialForAnalysis(this);
    }
    bool spMaterialTexture::UpdateTextureAnimationForAnalysis()
    {
        if(!animationController_)return true;
        return animationOwner_&&animationOwner_->UpdateForRenderForAnalysis();
    }

    void spMaterialTexture::SetUVControllerForAnalysis(
        spBaseObject* controller) noexcept
    {
        if(uvController_==controller){if(uvOwner_)uvOwner_->BindMaterialForAnalysis(this);return;}
        if(uvOwner_&&uvOwner_->GetMaterialForAnalysis()==this)uvOwner_->DetachMaterialForAnalysis();
        uvOwner_.reset(); // explicitly borrowed compatibility overload
        uvController_ = controller;
        if(!(textureStates_[8]&8))textureStates_[8]=controller?2u:0u;
    }

    void spMaterialTexture::SetOwnedUVControllerForAnalysis(std::shared_ptr<spUVController> controller) noexcept
    {
        if(uvOwner_!=controller&&uvOwner_&&uvOwner_->GetMaterialForAnalysis()==this)uvOwner_->DetachMaterialForAnalysis();
        uvOwner_=std::move(controller);uvController_=uvOwner_.get();
        if(uvOwner_)uvOwner_->BindMaterialForAnalysis(this);
        if(!(textureStates_[8]&8))textureStates_[8]=uvController_?2u:0u;
    }
    bool spMaterialTexture::UpdateUVAnimationForAnalysis()
    {
        if(!uvController_)return true;
        if(!uvOwner_)return false;
        return uvOwner_->GetAppliedTimeForAnalysis()==uvOwner_->GetAccumulatedTimeForAnalysis()
            ||uvOwner_->UpdateForRenderForAnalysis();
    }

    bool spMaterialTexture::UpdateForRenderForAnalysis(const std::uint32_t stage,const UVSubmitForAnalysis submit,void* context)
    {
        if(!UpdateUVAnimationForAnalysis())return false;
        if(uvController_||hasStaticUV_)
        {
            if(!submit)return false; // explicit host guard
            (void)submit(context,stage,uvTransform_);
        }
        return UpdateTextureAnimationForAnalysis();
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
