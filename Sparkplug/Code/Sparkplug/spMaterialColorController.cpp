#include "spMaterialColorController.h"
#include "Analysis/PC/spColorMath.h"
#include <algorithm>
#include <cmath>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spMaterialColorController::StaticRTTI() noexcept
    {
        // No host resource factory until original factory/defaults are proven.
        // Explicit declared objects can be passed to the shared payload codec.
        static const spRTTIRecord record{ClassID,spRenderController::ClassID,"spMaterialColorController",&spRenderController::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);(void)registered;return record;
    }
    const spRTTIRecord& spMaterialColorController::vfunc_18() const noexcept{return StaticRTTI();}
    void spMaterialColorController::BindMaterialForAnalysis(spMaterial* material) noexcept
    {
        if(material_&&material_!=material)
        {
            material_->SetAmbientColorForAnalysis(saved_[0]);material_->SetDiffuseColorForAnalysis(saved_[1]);
            material_->SetSpecularColorForAnalysis(saved_[2]);material_->SetEmissiveColorForAnalysis(saved_[3]);
        }
        material_=material;
        if(material_)saved_={material_->GetAmbientColorForAnalysis(),material_->GetDiffuseColorForAnalysis(),material_->GetSpecularColorForAnalysis(),material_->GetEmissiveColorForAnalysis()};
    }
    bool spMaterialColorController::UpdateForRenderForAnalysis()
    {
        const float elapsed=GetAccumulatedTimeForAnalysis()-GetAppliedTimeForAnalysis();
        if(!material_||!std::isfinite(elapsed))return false;
        const auto active=[](const spFunctionEval& f){return f.GetStateForAnalysis().functionType!=0;};
        const auto evaluate=[&](spColorFuncEval& color,spMaterial::ColorRGBA& out)
        {
            std::uint32_t argb=0;if(!color.EvaluateColorForAnalysis(elapsed,argb))return false;
            out=PCARGBToRGBAForAnalysis(argb);return true;
        };
        spMaterial::ColorRGBA value;
        if(active(colors_[0].GetFunctionForAnalysis()))
        {if(!evaluate(colors_[0],value))return false;material_->SetAmbientColorForAnalysis(value);}
        auto diffuse=material_->GetDiffuseColorForAnalysis();const float oldAlpha=diffuse[3];
        if(active(colors_[1].GetFunctionForAnalysis()))
        {if(!evaluate(colors_[1],diffuse))return false;diffuse[3]=oldAlpha;}
        if(active(alpha_))
        {double alpha=0;if(!alpha_.EvaluateExtendedForAnalysis(elapsed,alpha))return false;diffuse[3]=static_cast<float>(std::clamp(alpha,0.,1.));}
        material_->SetDiffuseColorForAnalysis(diffuse); // native unconditional setter
        if(active(colors_[2].GetFunctionForAnalysis()))
        {if(!evaluate(colors_[2],value))return false;material_->SetSpecularColorForAnalysis(value);}
        if(active(colors_[3].GetFunctionForAnalysis()))
        {if(!evaluate(colors_[3],value))return false;material_->SetEmissiveColorForAnalysis(value);}
        (void)ConsumeElapsedForAnalysis();return true;
    }
}
