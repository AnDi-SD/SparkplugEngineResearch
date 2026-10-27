#include "spProjectionManager.h"
#include "Code/SparkplugPC/spPCProjectionFX.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord record{spProjectionManager::ClassID,spCrossPlatform::ClassID,
            "spProjectionManager",&spCrossPlatform::StaticRTTI(),nullptr,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        void* Require(void* value)
        {if(!value)throw std::logic_error("Projection initialization requires a live borrowed handle");return value;}
    }
    bool InitializeProjectionPCForAnalysis(void* projection,spProjectionManagerHost& host)
    {
        Require(projection);
        if(!host.ReadProjectionHelper94ForAnalysis(projection))
            throw std::logic_error("Projection helper94 factory4C51B0 is outside the qualified contract");
        // The original reloads94 before its owner store, then reloads it again
        // before the virtual call. Borrowed fields may alias or be replaced.
        auto* helper=Require(host.ReadProjectionHelper94ForAnalysis(projection));
        host.WriteProjectionHelperOwner18ForAnalysis(helper,projection);
        helper=Require(host.ReadProjectionHelper94ForAnalysis(projection));
        (void)host.InvokeProjectionHelper0ForAnalysis(helper);
        return true; // literal AL=1, regardless of the foreign helper result
    }
    spProjectionFxInitializationResultForAnalysis InitializeProjectionPCWithOwnedFxForAnalysis(
        void* projection,std::unique_ptr<spPCProjectionFX>& helper94,const spProjectionFxHost& host)
    {
        if(!projection)return spProjectionFxInitializationResultForAnalysis::UnqualifiedMissingOwner;
        if(!helper94)helper94=std::make_unique<spPCProjectionFX>();
        helper94->BindOwnerForAnalysis(projection);
        // Never turn an unqualified material prefix into the native wrapper's
        // literal successful result by discarding the analytical FX status.
        return helper94->InitializeForAnalysis(host);
    }
    const spRTTIRecord& spProjectionManager::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spProjectionManager::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spProjectionManager::vfunc_10(spCloneManager&) const{return nullptr;}
    bool spProjectionManager::EnableForAnalysis() noexcept{enabled_=1;return true;}
    void spProjectionManager::AddProjectionForAnalysis(void* value)
    {
        if(std::find(projections_.begin(),projections_.end(),value)==projections_.end())
            projections_.push_back(value);
    }
    void spProjectionManager::RemoveProjectionForAnalysis(void* value) noexcept
    {
        for(auto it=projections_.begin();it!=projections_.end();)
        {
            if(*it!=value){++it;continue;}
            const auto* address=std::addressof(*it);
            for(auto* frame:activeFrames_)if(frame->current==address)frame->erased=true;
            it=projections_.erase(it);
        }
    }
    void spProjectionManager::SetProjectionsForFixture(std::list<void*> values)
    {
        if(!activeFrames_.empty())throw std::logic_error("Cannot replace a projection list during traversal");
        projections_=std::move(values);
    }
    bool spProjectionManager::WalkForAnalysis(const std::function<std::uint8_t(void*)>& call)
    {
        std::uint8_t result=1;std::size_t visits=0;
        for(auto it=projections_.begin();it!=projections_.end();)
        {
            if(visits++>=65536||activeFrames_.size()>=128)
                throw std::logic_error("Projection traversal exceeds its analytical visit/depth bound");
            Frame frame{std::addressof(*it),false};activeFrames_.push_back(&frame);
            struct Pop final{std::vector<Frame*>& frames;~Pop(){frames.pop_back();}} pop{activeFrames_};
            result&=call(*it); // literal low-byte AND; do not normalize leaf results
            if(frame.erased)
                throw std::logic_error("Projection traversal current list node was erased");
            ++it; // only after the guard, never increment an erased iterator
        }
        return result!=0;
    }
    bool spProjectionManager::InitializeProjectionsForAnalysis(spProjectionManagerHost& host)
    {
        return WalkForAnalysis([&](void* projection){return static_cast<std::uint8_t>(InitializeProjectionPCForAnalysis(projection,host));});
    }
    bool spProjectionManager::DrawProjectionsForAnalysis(void* camera,spProjectionManagerHost& host)
    {
        return WalkForAnalysis([&](void* projection){return static_cast<std::uint8_t>(host.DrawProjection38ForAnalysis(Require(projection),camera));});
    }
}
