#pragma once
#include "spRenderNode.h"

namespace sparkplug::reconstruction
{
    // PC49E4C0 constructs the inherited 1D4 storage and replaces both tables.
    // The engine registers spRenderNodeSerializer for this exact class (6D4B00).
    class spSkyBox : public spRenderNode
    {
    public:
        static constexpr spClassID ClassID=0x7A7124AF;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        [[nodiscard]] bool UpdateWorldForAnalysis(std::uint32_t inheritedFlags=0,
            const Matrix3* cameraOrientation=nullptr) noexcept override;

        // Native secondary support4D74A0 is true/no-op. Dedicated49E5F0 first
        // clears every Fog, then calls the BASE support424B60 with force=false.
        // The callback is that explicit unrepresented renderer boundary.
        using BaseSupportDrawForAnalysis=bool(*)(void*,spRenderNode&,bool);
        [[nodiscard]] bool RenderNormalForAnalysis() const noexcept{return true;}
        [[nodiscard]] bool RenderSkyForAnalysis(BaseSupportDrawForAnalysis,void*);
    };
}
