#include "spColorFuncEval.h"
#include <algorithm>
#include <cmath>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spColorFuncEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spColorEval::ClassID,"spColorFuncEval",&spColorEval::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spColorFuncEval>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);(void)registered;return record;
    }
    const spRTTIRecord& spColorFuncEval::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spColorFuncEval::vfunc_10(spCloneManager& manager) const
    {
        // Original478AC0 uses inherited Base40ECE0: default, not state copy.
        auto result=std::make_unique<spColorFuncEval>();manager.RegisterClone(*this,*result);
        return spBaseObject::vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    bool spColorFuncEval::EvaluateColorForAnalysis(float delta,std::uint32_t& argb)
    {
        double value=0;if(!function_.EvaluateExtendedForAnalysis(delta,value))return false;
        const double shifted=(value+1.0)*.5;
        if(!std::isfinite(shifted))return false;
        const float weight=std::clamp(static_cast<float>(shifted),0.F,1.F);
        const float opposite=static_cast<float>(1.0-static_cast<double>(weight));
        std::uint32_t color=0;
        for(unsigned shift=0;shift<32;shift+=8)
        {
            // Native478920 truncates EACH scaled byte through60DB90 before
            // adding bytes, not one rounded/saturating combined lerp.
            const auto first=static_cast<std::uint32_t>(static_cast<double>((color1_>>shift)&255u)*weight);
            const auto second=static_cast<std::uint32_t>(static_cast<double>((color2_>>shift)&255u)*opposite);
            color|=((first+second)&255u)<<shift;
        }
        argb=color;return true;
    }
}
