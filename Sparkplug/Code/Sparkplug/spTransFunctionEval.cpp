#include "spTransFunctionEval.h"
#include "Analysis/PC/spAnimationMath.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include <cmath>
namespace sparkplug::reconstruction
{
    spTransFunctionEval::spTransFunctionEval() noexcept
    {for(std::size_t i=3;i<6;++i){auto state=functions_[i].GetStateForAnalysis();state.yOffset=1;functions_[i].SetStateForAnalysis(state);}}
    const spRTTIRecord& spTransFunctionEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spTransformEval::ClassID,"spTransFunctionEval",&spTransformEval::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spTransFunctionEval>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);(void)registered;return record;
    }
    const spRTTIRecord& spTransFunctionEval::vfunc_18() const noexcept{return StaticRTTI();}
    spTransformEval::SampleForAnalysis spTransFunctionEval::EvaluateForAnalysis(float delta)
    {SampleForAnalysis result{};if(!EvaluateSampleForAnalysis(delta,result))return {};return result;}
    bool spTransFunctionEval::EvaluateSampleForAnalysis(float delta,SampleForAnalysis& result,spFunctionEval::RandomStateForAnalysis* random)
    {
        for(float v:axis_)if(!std::isfinite(v))return false;
        std::array<float,7> values{};double rotationValue=0;
        // Original47CD10 order matters for the shared global random stream.
        for(const auto i:{2,1,0,6,5,4,3})
        {
            if(i==6){if(!functions_[i].EvaluateExtendedForAnalysis(delta,rotationValue,random))return false;}
            else if(!functions_[i].EvaluateForAnalysis(delta,values[i],random))return false;
        }
        const float angle=static_cast<float>(rotationValue*6.2831854820251465F);
        const double half=static_cast<double>(angle)*0.5,sine=std::sin(half);
        if(!std::isfinite(angle))return false;
        SampleForAnalysis sample;sample.position={values[0],values[1],values[2]};sample.scale={values[3],values[4],values[5]};
        sample.rotation={static_cast<float>(sine*axis_[0]),static_cast<float>(sine*axis_[1]),static_cast<float>(sine*axis_[2]),static_cast<float>(std::cos(half))};
        sample.hasPosition=sample.hasRotation=sample.hasScale=true;result=sample;return true;
    }
    bool spTransFunctionEval::EvaluateMatrixForAnalysis(float delta,Matrix4& result,spFunctionEval::RandomStateForAnalysis* random)
    {
        namespace math=sparkplug::evidence::pc::node_math;
        for(float v:pivot_)if(!std::isfinite(v))return false;
        SampleForAnalysis sample;if(!EvaluateSampleForAnalysis(delta,sample,random))return false;
        const auto rotation=sparkplug::evidence::pc::animation_math::ToMatrix(sample.rotation);
        const auto translation=[](const Vector3& p){Matrix4 m{1,0,0,0,0,1,0,0,0,0,1,0,p[0],p[1],p[2],1};return m;};
        Matrix4 value=translation({-pivot_[0],-pivot_[1],-pivot_[2]});
        const Matrix4 r{rotation[0],rotation[1],rotation[2],0,rotation[3],rotation[4],rotation[5],0,rotation[6],rotation[7],rotation[8],0,0,0,0,1};
        value=math::Multiply4ForAnalysis(value,r);
        value=math::Multiply4ForAnalysis(value,translation(pivot_));
        value=math::Multiply4ForAnalysis(value,translation(sample.position));
        const Matrix4 scale{sample.scale[0],0,0,0,0,sample.scale[1],0,0,0,0,sample.scale[2],0,0,0,0,1};
        value=math::Multiply4ForAnalysis(value,scale);
        for(float v:value)if(!std::isfinite(v))return false;result=value;return true;
    }
}
