#include "spProjection.h"
#include "spProjectionManager.h"
#include "Code/SparkplugPC/spPCProjectionFX.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::optional<spProjectionConstructorDataForAnalysis> factoryData;
        const spRTTIRecord record{spProjection::ClassID,spRenderable::ClassID,
            "spProjection",&spRenderable::StaticRTTI(),nullptr,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        class MaterialData final : public spProjectionFxHost
        {
        public:
            explicit MaterialData(const spProjection& projection):projection_(projection){}
            const void* ReadProjectionMaterial20ForAnalysis(const void* owner) const noexcept override
            {
                if(owner!=&projection_)std::terminate();
                return projection_.GetMaterialForAnalysis().get();
            }
        private:
            const spProjection& projection_;
        };
    }
    spProjection::spProjection(spProjectionConstructorDataForAnalysis data)
    {SetField28ForAnalysis(data.debugField28);}
    spProjection::~spProjection()=default;
    const spRTTIRecord& spProjection::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spProjection::vfunc_18() const noexcept{return record;}
    void spProjection::SetFactoryConstructorDataForAnalysis(
        std::optional<spProjectionConstructorDataForAnalysis> data) noexcept{factoryData=data;}
    spProjectionConstructorDataForAnalysis spProjection::RequireFactoryConstructorDataForAnalysis()
    {
        if(!factoryData)throw std::logic_error("Projection factory requires captured common Debug constructor data");
        return *factoryData;
    }
    spProjectionFxInitializationResultForAnalysis spProjection::InitializePCForAnalysis()
    {return InitializeProjectionPCWithOwnedFxForAnalysis(this,helper94_,MaterialData(*this));}
    const spRenderable::BoundingSphere& spProjection::GetBoundingSphereForAnalysis() const noexcept
    {RefreshProjectionSphereForAnalysis();return state_.sphere70;}
    void spProjection::GetBoundsForAnalysis(BoundsPosition& minimum,BoundsPosition& maximum) const noexcept
    {ReadProjectionBoundsForAnalysis(minimum,maximum);}
}
