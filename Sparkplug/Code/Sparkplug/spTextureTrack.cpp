#include "spTextureTrack.h"
#include "spTexture.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace sparkplug::reconstruction
{
    const spRTTIRecord& spTextureTrack::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spTrack::ClassID,"spTextureTrack",&spTrack::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spTextureTrack>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spTextureTrack::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spTextureTrack::vfunc_10(spCloneManager&) const{return nullptr;} // pending clone proof
    float spTextureTrack::GetDurationForAnalysis() const noexcept{return times_.empty()?0:times_.back();}
    void spTextureTrack::ReleaseKeysForAnalysis() noexcept{times_.clear();textures_.clear();}
    bool spTextureTrack::SetKeysForAnalysis(std::vector<float> times,std::vector<std::shared_ptr<spTexture>> textures)
    {
        if(times.size()!=textures.size()||times.size()>MaximumKeysForAnalysis)return false;
        times_=std::move(times);textures_=std::move(textures);return true;
    }
    bool spTextureTrack::EvaluateForAnalysis(float time,std::shared_ptr<spTexture>& output) const noexcept
    {
        if(!std::isfinite(time)||!std::is_sorted(times_.begin(),times_.end())
            ||!std::all_of(times_.begin(),times_.end(),[](float value){return std::isfinite(value);}))return false;
        if(times_.empty()){output.reset();return true;}
        // Actual478C60: last endpoint is inclusive, earlier endpoints select
        // the FIRST key strictly greater than time (end-time, not start-time).
        if(time>=times_.back()){output=textures_.back();return true;}
        const auto found=std::upper_bound(times_.begin(),times_.end(),time);
        if(found==times_.end())return false; // original fallback slot4 is unsafe
        output=textures_[static_cast<std::size_t>(found-times_.begin())];return true;
    }
}
