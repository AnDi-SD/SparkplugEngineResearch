#include "spColorEval.h"
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spColorEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spEvaluator::ClassID,"spColorEval",&spEvaluator::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);(void)registered;return record;
    }
    const spRTTIRecord& spColorEval::vfunc_18() const noexcept{return StaticRTTI();}
}
