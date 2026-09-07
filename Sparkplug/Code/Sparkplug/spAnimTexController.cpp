#include "spAnimTexController.h"
#include "spMaterialTexture.h"
#include <cmath>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spAnimTexController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spRenderController::ClassID,"spAnimTexController",
            &spRenderController::StaticRTTI(),+[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spAnimTexController>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spAnimTexController::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spAnimTexController::vfunc_10(spCloneManager&) const{return nullptr;} // pending native graph/array clone safety
    bool spAnimTexController::UpdateForRenderForAnalysis()
    {
        const float duration=track_.GetDurationForAnalysis();
        const float pending=GetAccumulatedTimeForAnalysis()-GetAppliedTimeForAnalysis();
        const float next=playbackTime_+pending;
        if(!material_||!std::isfinite(next)||!std::isfinite(duration)||duration<=0)return false;
        // Native repeated subtraction is strictly >, not >=; exact duration
        // stays at its last key. No negative wrapping. Limit the host work,
        // rather than reproducing zero/negative-duration infinite loops.
        if(next>duration&&static_cast<double>(next)/duration>4096)return false;
        double wrapped=next;
        while(wrapped>duration)wrapped-=duration;
        std::shared_ptr<spTexture> texture;
        if(!track_.EvaluateForAnalysis(static_cast<float>(wrapped),texture))return false;
        (void)ConsumeElapsedForAnalysis();playbackTime_=static_cast<float>(wrapped);
        material_->SetOwnedFallBackTextureForAnalysis(std::move(texture));return true;
    }
}
