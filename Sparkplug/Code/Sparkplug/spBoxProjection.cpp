#include "spBoxProjection.h"
#include <cmath>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create()
        {return std::make_unique<spBoxProjection>();}
        const spRTTIRecord record{spBoxProjection::ClassID,spProjection::ClassID,
            "spBoxProjection",&spProjection::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool Admitted(float value) noexcept
        {return std::isfinite(value)&&std::abs(value)<=1024.0f&&std::trunc(value)==value;}
    }
    spBoxProjection::spBoxProjection():spBoxProjection(RequireFactoryConstructorDataForAnalysis()){}
    spBoxProjection::spBoxProjection(spProjectionConstructorDataForAnalysis data):spProjection(data){}
    const spRTTIRecord& spBoxProjection::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spBoxProjection::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spBoxProjection::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<spBoxProjection>(RequireFactoryConstructorDataForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*result);
        return vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    bool spBoxProjection::SetGeometryForFixture(const Vector3& center,const Vector3& size) noexcept
    {
        for(unsigned i=0;i!=3;++i)if(!Admitted(center[i])||!Admitted(size[i]))return false;
        const auto capturedCenter=center;
        const auto capturedSize=size;
        center9C_=capturedCenter;sizeA8_=capturedSize;return true;
    }
    void spBoxProjection::RefreshProjectionSphereForAnalysis() const noexcept
    {
        for(unsigned i=0;i!=3;++i)state_.sphere70[i]=center9C_[i];
        const double x=sizeA8_[0],y=sizeA8_[1],z=sizeA8_[2];
        // In the admitted integer domain products/sum are exact (<2^24).
        // Square-root rounding tobinary64 thenbinary32 has the same result
        // as the native nearest64 accumulator for these integer radicands.
        state_.sphere70[3]=static_cast<float>(std::sqrt((x*x+y*y)+z*z)*0.5);
    }
    void spBoxProjection::ReadProjectionBoundsForAnalysis(BoundsPosition& minimum,BoundsPosition& maximum) const noexcept
    {
        for(unsigned i=0;i!=3;++i)
        {
            // Preserve the native X wide product versus Y/Z float spills.
            // Negative extents intentionally leave the bounds unsorted.
            const double negative=i==0?double(sizeA8_[i])*-0.5:double(float(double(sizeA8_[i])*-0.5));
            const double positive=i==0?double(sizeA8_[i])*0.5:double(float(double(sizeA8_[i])*0.5));
            minimum[i]=float(negative+double(center9C_[i]));
            maximum[i]=float(positive+double(center9C_[i]));
        }
    }
}
