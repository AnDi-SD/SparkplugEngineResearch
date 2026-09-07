#include "spMaterialTextureLayer.h"

#include "spMaterialTexture.h"

#include <utility>
#include <algorithm>

namespace sparkplug::reconstruction
{
    bool spMaterialTextureLayer::UpdateForRenderForAnalysis(std::uint32_t stage,UVSubmitForAnalysis submit,void* context)
    {return materialTexture_&&materialTexture_->UpdateForRenderForAnalysis(stage,submit,context);}
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialTextureLayer()
        {
            return std::make_unique<spMaterialTextureLayer>();
        }

        const spRTTIRecord MaterialTextureLayerRecord{
            spMaterialTextureLayer::ClassID,
            spBaseObject::ClassID,
            "spMaterialTextureLayer",
            &spBaseObject::StaticRTTI(),
            &CreateMaterialTextureLayer,
            nullptr,
        };

        const bool MaterialTextureLayerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialTextureLayerRecord);
    }

    spMaterialTextureLayer::spMaterialTextureLayer() noexcept = default;
    spMaterialTextureLayer::~spMaterialTextureLayer() = default;

    bool spMaterialTextureLayer::CopyTextureStatesForAnalysis(std::uint32_t,
        std::array<std::uint32_t,9>& output) const noexcept
    {
        if(!materialTexture_)return false;
        const auto& states=materialTexture_->GetTextureStatesForAnalysis();
        std::copy_n(states.begin(),output.size(),output.begin());return true;
    }

    const spRTTIRecord& spMaterialTextureLayer::StaticRTTI() noexcept
    {
        (void)MaterialTextureLayerRegistered;
        return MaterialTextureLayerRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialTextureLayer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialTextureLayer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialTextureLayer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target = dynamic_cast<spMaterialTextureLayer*>(&destination);
        if (target == nullptr || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }
        target->materialTexture_.reset();
        if (materialTexture_ == nullptr)
        {
            return true;
        }

        auto clonedBase = materialTexture_->vfunc_10(manager);
        auto* const clonedTexture =
            dynamic_cast<spMaterialTexture*>(clonedBase.get());
        if (clonedTexture == nullptr)
        {
            return false;
        }
        clonedBase.release();
        target->materialTexture_.reset(clonedTexture);
        return true;
    }

    const spRTTIRecord& spMaterialTextureLayer::vfunc_18() const noexcept
    {
        return MaterialTextureLayerRecord;
    }

    const std::unique_ptr<spMaterialTexture>&
    spMaterialTextureLayer::GetMaterialTextureForAnalysis() const noexcept
    {
        return materialTexture_;
    }

    void spMaterialTextureLayer::SetMaterialTextureForAnalysis(
        std::unique_ptr<spMaterialTexture> texture) noexcept
    {
        materialTexture_ = std::move(texture);
    }
}
