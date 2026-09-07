#pragma once
// Original class and logical dependency chain. Exact original path/API unknown.
#include "spRenderController.h"
#include "spTextureTrack.h"
namespace sparkplug::reconstruction
{
    class spMaterialTexture;
    class spAnimTexController final : public spRenderController
    {
    public:
        static constexpr spClassID ClassID=0x16FB0E47;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;} // unresolved graph/array clone
        void BindMaterialForAnalysis(spMaterialTexture* material) noexcept{material_=material;}
        [[nodiscard]] spMaterialTexture* GetMaterialForAnalysis() const noexcept{return material_;}
        [[nodiscard]] spTextureTrack& GetTextureTrackForAnalysis() noexcept{return track_;}
        [[nodiscard]] const spTextureTrack& GetTextureTrackForAnalysis() const noexcept{return track_;}
        [[nodiscard]] float GetPlaybackTimeForAnalysis() const noexcept{return playbackTime_;}
        [[nodiscard]] bool UpdateForRenderForAnalysis() override;
    private:
        spMaterialTexture* material_=nullptr; // borrowed PC24, NOT an owning cycle
        spTextureTrack track_;               // PC28..47
        float playbackTime_=0;               // PC48
    };
}
