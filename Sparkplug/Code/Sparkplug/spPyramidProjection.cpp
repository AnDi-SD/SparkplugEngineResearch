#include "spPyramidProjection.h"
#include <cmath>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPyramidProjection>();}
        const spRTTIRecord record{spPyramidProjection::ClassID,spProjection::ClassID,
            "spPyramidProjection",&spProjection::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool Integer(float value) noexcept
        {return std::isfinite(value)&&std::abs(value)<=1024.0f&&std::trunc(value)==value;}
    }
    spPyramidProjection::spPyramidProjection():spPyramidProjection(RequireFactoryConstructorDataForAnalysis()){}
    spPyramidProjection::spPyramidProjection(spProjectionConstructorDataForAnalysis data):spProjection(data){}
    const spRTTIRecord& spPyramidProjection::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spPyramidProjection::vfunc_18() const noexcept{return record;}
    spPyramidProjection::SerializedFieldsForAnalysis spPyramidProjection::GetSerializedFieldsForAnalysis() noexcept
    {return {own_.flip9C,centerA0_,sizeAC_};}
    std::unique_ptr<spBaseObject> spPyramidProjection::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<spPyramidProjection>(RequireFactoryConstructorDataForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*result);
        return vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    bool spPyramidProjection::SetGeometryForFixture(const Vector3& center,const Vector3& size) noexcept
    {
        for(unsigned i=0;i<3;++i)if(!Integer(center[i])||!Integer(size[i]))return false;
        const auto height=static_cast<std::uint32_t>(std::abs(size[1]));
        if(height==0||(height&(height-1))!=0)return false;
        const auto nextCenter=center,nextSize=size;
        centerA0_=nextCenter;sizeAC_=nextSize;return true;
    }
    void spPyramidProjection::RefreshProjectionSphereForAnalysis() const noexcept
    {
        const double width=sizeAC_[0],height=sizeAC_[1],depth=sizeAC_[2];
        // The admitted numerator4*h*h+w*w+d*d is<2^23 and the denominator
        // is a power of two. Radius and Y center are exactly representable.
        const double radius=((width*width+depth*depth)/height)*0.125+height*0.5;
        state_.sphere70[3]=static_cast<float>(radius);
        state_.sphere70[0]=0.0f;state_.sphere70[2]=0.0f;
        state_.sphere70[1]=static_cast<float>(height-radius);
        if(own_.flip9C!=0)
        {
            const float z=-state_.sphere70[2],x=-state_.sphere70[0],y=-state_.sphere70[1];
            state_.sphere70[2]=z;state_.sphere70[0]=x;state_.sphere70[1]=y;
        }
    }
    void spPyramidProjection::ReadProjectionBoundsForAnalysis(BoundsPosition& minimum,BoundsPosition& maximum) const noexcept
    {
        for(unsigned i=0;i<3;++i)
        {
            const double negative=i==0?double(sizeAC_[i])*-0.5:double(float(double(sizeAC_[i])*-0.5));
            const double positive=i==0?double(sizeAC_[i])*0.5:double(float(double(sizeAC_[i])*0.5));
            minimum[i]=float(negative+double(centerA0_[i]));
            maximum[i]=float(positive+double(centerA0_[i]));
        }
    }
}
