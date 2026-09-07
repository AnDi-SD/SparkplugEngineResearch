#include "spDXShaderLayer.h"
#include <utility>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXShaderLayer>();}
        const spRTTIRecord Record{spDXShaderLayer::ClassID,spMaterialTextureLayer::ClassID,"spDXShaderLayer",&spMaterialTextureLayer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXShaderLayer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXShaderLayer::vfunc_18() const noexcept{return Record;}
    void spDXShaderLayer::AppendParameterPairForAnalysis(ParameterPairForAnalysis value){parameters_.push_back(std::move(value));}
    void spDXShaderLayer::ClearParameterPairsForAnalysis() noexcept
    {std::vector<ParameterPairForAnalysis>().swap(parameters_);}
    std::unique_ptr<spBaseObject> spDXShaderLayer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXShaderLayer>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spDXShaderLayer::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<spDXShaderLayer*>(&destination);
        if(!target||target==this)return false; // host alias guard, not native parity
        target->ClearParameterPairsForAnalysis();
        if(!spMaterialTextureLayer::vfunc_14(destination,manager))return false;
        target->shaderWord_=shaderWord_;
        target->parameters_=parameters_; // both float4 arrays deep-copied
        return true;
    }
}
