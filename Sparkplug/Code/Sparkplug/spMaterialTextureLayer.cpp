#include "spMaterialTextureLayer.h"

#include "spMaterialTexture.h"

#include <utility>

namespace sparkplug::reconstruction
{
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
            spRTTIManager::Instance().Register(MaterialTextureLayerRecord);
    }

    spMaterialTextureLayer::~spMaterialTextureLayer() = default;

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

    const std::shared_ptr<spMaterialTexture>&
    spMaterialTextureLayer::GetMaterialTextureForAnalysis() const noexcept
    {
        return materialTexture_;
    }

    void spMaterialTextureLayer::SetMaterialTextureForAnalysis(
        std::shared_ptr<spMaterialTexture> texture) noexcept
    {
        materialTexture_ = std::move(texture);
    }
}
