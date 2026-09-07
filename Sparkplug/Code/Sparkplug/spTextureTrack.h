#pragma once
// Original class/RTTI. PC embedded track extent20, times14/owners18/count1C.
#include "spTrack.h"
#include <memory>
#include <vector>
namespace sparkplug::reconstruction
{
    class spTexture;
    class spTextureTrack final : public spTrack
    {
    public:
        static constexpr spClassID ClassID=0x0B3C1B09;
        static constexpr std::size_t MaximumKeysForAnalysis=4096; // host bound
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;} // no fabricated key copy
        [[nodiscard]] float GetDurationForAnalysis() const noexcept override;
        void ReleaseKeysForAnalysis() noexcept override;
        // Serialization preserves raw times, including malformed timelines;
        // runtime evaluation separately rejects nonfinite/unsorted inputs.
        bool SetKeysForAnalysis(std::vector<float> times,std::vector<std::shared_ptr<spTexture>> textures);
        [[nodiscard]] bool EvaluateForAnalysis(float time,std::shared_ptr<spTexture>& output) const noexcept;
        [[nodiscard]] const auto& GetTimesForAnalysis() const noexcept{return times_;}
        [[nodiscard]] const auto& GetTexturesForAnalysis() const noexcept{return textures_;}
    private:
        std::vector<float> times_;
        std::vector<std::shared_ptr<spTexture>> textures_;
    };
}
