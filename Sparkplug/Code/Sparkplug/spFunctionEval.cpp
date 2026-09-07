#include "spFunctionEval.h"
#include <cmath>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spFunctionEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spEvaluator::ClassID,"spFunctionEval",&spEvaluator::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spFunctionEval>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spFunctionEval::vfunc_18() const noexcept{return StaticRTTI();}
    spFunctionEval::RandomStateForAnalysis& spFunctionEval::SharedRandomForAnalysis()
    {static RandomStateForAnalysis state;return state;}
    std::unique_ptr<spBaseObject> spFunctionEval::vfunc_10(spCloneManager& manager) const
    {
        // Native4788D0 invokes inherited Base40ECE0, leaving factory defaults.
        // Embedded UV deep-copy434940 is a different, separately studied path.
        auto result=std::make_unique<spFunctionEval>();manager.RegisterClone(*this,*result);
        return spBaseObject::vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    bool spFunctionEval::EvaluateForAnalysis(float delta,float& output,RandomStateForAnalysis* random)
    {double value=0;if(!EvaluateExtendedForAnalysis(delta,value,random))return false;output=static_cast<float>(value);return true;}
    bool spFunctionEval::EvaluateExtendedForAnalysis(float delta,double& output,RandomStateForAnalysis* random)
    {
        const auto& s=state_;
        if(s.functionType==0||s.functionType==7)
        {if(!std::isfinite(s.yOffset))return false;output=s.yOffset;return true;}
        if(!std::isfinite(delta)||!std::isfinite(s.time)||!std::isfinite(s.frequency)||s.frequency==0
            ||!std::isfinite(s.reciprocal)||!std::isfinite(s.amplitude)||!std::isfinite(s.xOffset)
            ||!std::isfinite(s.yOffset)||!std::isfinite(s.pitch)||(s.clampEnabled&&!std::isfinite(s.clampLimit)))return false;
        // Match explicit original FST/FSTP float boundaries. Double models
        // retained x87 intermediates for bounded finite tests; no universal
        // bit-exact80-bit or non-default rounding-mode claim.
        const double accumulated=static_cast<double>(s.time)+delta;
        float nextTime=static_cast<float>(accumulated);
        const float shifted=static_cast<float>(accumulated+s.xOffset);
        const double periods=std::floor(accumulated*s.frequency);
        const float whole=static_cast<float>(periods*s.reciprocal);
        const float phase=static_cast<float>((static_cast<double>(shifted)-whole)*s.frequency);
        if(!std::isfinite(nextTime)||!std::isfinite(whole)||!std::isfinite(phase))return false;
        double value=0;
        switch(s.functionType)
        {
        case 1:value=std::sin(static_cast<double>(6.2831854820251465F)*phase);break;
        case 2:value=phase<0.5F?1.0:-1.0;break;
        case 3:value=static_cast<double>(phase)-0.5;break;
        case 4:value=0.5-static_cast<double>(phase);break;
        case 5:value=phase<0.5F?2.0*phase-0.5:0.5-2.0*(static_cast<double>(phase)-0.5);break;
        case 6:
            value=static_cast<double>((random?*random:SharedRandomForAnalysis()).Next()%10001U)
                *static_cast<double>(0.00019999999494757503F)-1.0;
            break;
        case 8:
            value=static_cast<double>(s.pitch)*nextTime+s.yOffset;
            // Native zero pitch does NOT enter either directional clamp.
            if(s.clampEnabled&&((s.pitch<0&&value<s.clampLimit)||(s.pitch>0&&value>=s.clampLimit)))value=s.clampLimit;
            break;
        default:break; // unknown IDs return amplitude*0+YOffset, time still advances
        }
        if(s.functionType>=1&&s.functionType<=5&&nextTime>s.reciprocal)
            nextTime=static_cast<float>(static_cast<double>(nextTime)-whole);
        if(s.functionType!=8)value=value*s.amplitude+s.yOffset;
        const float result=static_cast<float>(value);
        if(!std::isfinite(result)||!std::isfinite(nextTime))return false;
        state_.time=nextTime;output=value;return true;
    }
}
