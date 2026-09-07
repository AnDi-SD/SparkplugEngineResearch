#include "spRenderController.h"
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spRenderController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spController::ClassID,"spRenderController",
            &spController::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spRenderController::vfunc_18() const noexcept{return StaticRTTI();}
    bool spRenderController::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<spRenderController*>(&destination);
        if(!target||!spController::vfunc_14(destination,manager))return false;
        target->appliedTime_=appliedTime_;target->time_=time_;return true;
    }
    void spRenderController::ApplyForAnalysis(float elapsed){time_+=elapsed;}
    float spRenderController::ConsumeElapsedForAnalysis() noexcept
    {const float result=time_-appliedTime_;appliedTime_=time_;return result;}
}
