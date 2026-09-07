#pragma once
// Original PC class identity/RTTI; inferred path. The animation manager adds
// elapsed time through slot1C; render consumers apply it later through slot20.
#include "spController.h"

namespace sparkplug::reconstruction
{
    class spRenderController : public spController
    {
    public:
        static constexpr spClassID ClassID=0x14477AC7;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        bool vfunc_14(spBaseObject& destination,spCloneManager& manager) const override;
        void ApplyForAnalysis(float elapsed) override; // actual423190
        [[nodiscard]] float GetAccumulatedTimeForAnalysis() const noexcept{return time_;}
        [[nodiscard]] float GetAppliedTimeForAnalysis() const noexcept{return appliedTime_;}
        // bool is a host safety result, not the original void slot signature.
        [[nodiscard]] virtual bool UpdateForRenderForAnalysis()=0;
    protected:
        [[nodiscard]] float ConsumeElapsedForAnalysis() noexcept; // actual4231A0
    private:
        float appliedTime_=0; // PC1C
        float time_=0;        // PC20
    };
}
