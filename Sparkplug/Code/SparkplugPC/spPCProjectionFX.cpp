#include "spPCProjectionFX.h"
#include "Code/Sparkplug/spMaterial.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spPCProjectionFX>(); }
        const spRTTIRecord record{spPCProjectionFX::ClassID,spProjectionFX::ClassID,
            "spPCProjectionFX",&spProjectionFX::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spPCProjectionFX::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& spPCProjectionFX::vfunc_18() const noexcept { return record; }
    spProjectionDrawGateResultForAnalysis spPCProjectionFX::EvaluateDrawGateForAnalysis(
        const spProjectionDrawGateInputForAnalysis& input) const noexcept
    {
        using Result=spProjectionDrawGateResultForAnalysis;
        if(!input.sceneStamp40 || input.supportMark118!=*input.sceneStamp40)return Result::Skipped;
        if((input.nodeFlagsB0&0x200U)==0)return Result::Skipped;
        return Result::UnqualifiedActiveDraw;
    }
    std::unique_ptr<spBaseObject> spPCProjectionFX::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<spPCProjectionFX>();
        manager.RegisterCloneForAnalysis(*this,*result);
        // Native secondary table uses the inherited NamedObject Copy. Owner,
        // initialized byte, cache and opaque PC fields retain factory state.
        return vfunc_14(*result,manager) ? std::move(result) : nullptr;
    }
    spProjectionFxInitializationResultForAnalysis spPCProjectionFX::InitializeForAnalysis(const spProjectionFxHost& host)
    {
        using Result=spProjectionFxInitializationResultForAnalysis;
        if (initialized_ != 0) return Result::Completed;
        if (!owner_) return Result::UnqualifiedMissingOwner;
        if (host.ReadProjectionMaterial20ForAnalysis(owner_))
            return Result::UnqualifiedMaterialInitialization;
        initialized_=1;
        return Result::Completed;
    }
    spProjectionMaterialPreparationPrefixResultForAnalysis
        spPCProjectionFX::PrepareMaterialLayersPrefixForAnalysis(spMaterial& material) noexcept
    {
        using Result=spProjectionMaterialPreparationPrefixResultForAnalysis;
        if(initialized_!=0)return Result::AlreadyInitialized;
        if(!owner_)return Result::UnqualifiedMissingOwner;
        material.SetVertexAlphaByteForAnalysis(1);
        auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));
        if(!pass)return Result::UnqualifiedMaterialGraph;
        const auto count=pass->GetLayerCountForAnalysis();
        for(std::size_t i=0;i<count;++i)
        {
            const auto& layer=pass->GetLayerForAnalysis(i);
            if(!layer)return Result::UnqualifiedMaterialGraph;
            auto* texture=layer->GetMaterialTextureForAnalysis().get();
            if(!texture)return Result::UnqualifiedMaterialGraph;
            texture->SetTextureStateForAnalysis(3,2);
            texture->SetTextureStateForAnalysis(4,2);
            texture->SetTextureStateForAnalysis(8,11);
            texture->SetTextureStateForAnalysis(7,10);
        }
        // Native next calls460E50 and continues material/texture work. The
        // prefix retains its effects and never claims that remaining success.
        return Result::UnqualifiedMaterialLayerCreation;
    }
}
