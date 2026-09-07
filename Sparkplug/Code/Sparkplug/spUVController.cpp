#include "spUVController.h"
#include "spMaterialTexture.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include <cmath>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spUVController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spRenderController::ClassID,"spUVController",&spRenderController::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spUVController>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);(void)registered;return record;
    }
    const spRTTIRecord& spUVController::vfunc_18() const noexcept{return StaticRTTI();}
    void spUVController::BindMaterialForAnalysis(spMaterialTexture* material) noexcept
    {
        if(material_&&material_!=material)material_->SetStaticUVTransformForAnalysis(saved_);
        material_=material;
        if(material_)saved_=material_->GetUVTransformForAnalysis(); // alias snapshots too
    }
    bool spUVController::UpdateForRenderForAnalysis()
    {
        const float elapsed=GetAccumulatedTimeForAnalysis()-GetAppliedTimeForAnalysis();
        if(!material_||!std::isfinite(elapsed))return false;
        for(float value:saved_)if(!std::isfinite(value))return false;
        spTransFunctionEval::Matrix4 evaluated;
        if(!transform_.EvaluateMatrixForAnalysis(elapsed,evaluated))return false;
        const spTransFunctionEval::Matrix4 baseline{saved_[0],saved_[1],0,0,saved_[3],saved_[4],0,0,0,0,1,0,saved_[6],saved_[7],0,1};
        const auto result=sparkplug::evidence::pc::node_math::Multiply4ForAnalysis(baseline,evaluated);
        for(float value:result)if(!std::isfinite(value))return false;
        (void)ConsumeElapsedForAnalysis();
        material_->SetStaticUVTransformForAnalysis({result[0],result[1],0,result[4],result[5],0,result[12],result[13],1});return true;
    }
}
