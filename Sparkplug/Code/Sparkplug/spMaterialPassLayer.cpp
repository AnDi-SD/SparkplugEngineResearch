#include "spMaterialPassLayer.h"

#include "spMaterialTextureLayer.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialPassLayer()
        {
            return std::make_unique<spMaterialPassLayer>();
        }

        const spRTTIRecord MaterialPassLayerRecord{
            spMaterialPassLayer::ClassID,
            spBaseObject::ClassID,
            "spMaterialPassLayer",
            &spBaseObject::StaticRTTI(),
            &CreateMaterialPassLayer,
            nullptr,
        };

        const bool MaterialPassLayerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialPassLayerRecord);
    }

    spMaterialPassLayer::spMaterialPassLayer() noexcept = default;
    spMaterialPassLayer::~spMaterialPassLayer() = default;

    bool spMaterialPassLayer::UpdateForRenderForAnalysis(std::uint32_t stage,UVSubmitForAnalysis submit,void* context)
    {
        for(std::size_t index=0;index<layerCount_;++index)
            if(!layers_[index]||!layers_[index]->UpdateForRenderForAnalysis(stage==0xffffffffu?static_cast<std::uint32_t>(index):stage,submit,context))return false;
        return true;
    }

    const spRTTIRecord& spMaterialPassLayer::StaticRTTI() noexcept
    {
        (void)MaterialPassLayerRegistered;
        return MaterialPassLayerRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialPassLayer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialPassLayer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialPassLayer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target = dynamic_cast<spMaterialPassLayer*>(&destination);
        if (target == nullptr || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }

        for(auto& layer:target->layers_)layer.reset();
        target->finalBlendOperation_ = finalBlendOperation_;
        target->layerCount_ = layerCount_;
        for (std::size_t index = 0; index < layerCount_; ++index)
        {
            if (layers_[index] == nullptr)
            {
                continue;
            }
            auto clonedBase = layers_[index]->vfunc_10(manager);
            auto* const clonedLayer =
                dynamic_cast<spMaterialTextureLayer*>(clonedBase.get());
            if (clonedLayer == nullptr)
            {
                for(auto& layer:target->layers_)layer.reset();
                target->layerCount_ = 0;
                return false;
            }
            clonedBase.release();
            target->layers_[index].reset(clonedLayer);
        }
        return true;
    }

    const spRTTIRecord& spMaterialPassLayer::vfunc_18() const noexcept
    {
        return MaterialPassLayerRecord;
    }

    std::uint32_t spMaterialPassLayer::GetFinalBlendOperationForAnalysis()
        const noexcept
    {
        return finalBlendOperation_;
    }

    void spMaterialPassLayer::SetFinalBlendOperationForAnalysis(
        const std::uint32_t operation) noexcept
    {
        finalBlendOperation_ = operation;
    }

    std::size_t spMaterialPassLayer::GetLayerCountForAnalysis() const noexcept
    {
        return layerCount_;
    }

    const std::unique_ptr<spMaterialTextureLayer>&
    spMaterialPassLayer::GetLayerForAnalysis(const std::size_t index) const noexcept
    {
        static const std::unique_ptr<spMaterialTextureLayer> Empty;
        return index < layerCount_ ? layers_[index] : Empty;
    }

    bool spMaterialPassLayer::SetLayerForAnalysis(
        const std::size_t index,
        std::unique_ptr<spMaterialTextureLayer> layer) noexcept
    {
        if (index >= layers_.size())
        {
            return false;
        }
        layers_[index] = std::move(layer);
        // PC45F5E0 grows count even forNULL and never shifts/shrinks. This is
        // deliberately different from Material423960's pass-slot removal.
        layerCount_ = std::max(layerCount_, index + 1);
        return true;
    }
}
