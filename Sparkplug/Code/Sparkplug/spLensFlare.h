#pragma once
#include "spRenderable.h"
#include "spQuad.h"
namespace sparkplug::reconstruction {
class spRenderNode;
class spLensFlare : public spRenderable {
public:
    static constexpr spClassID ClassID=0x435370B5;
    // Anonymous native 0x58 record: dispatch pointer, actual spQuad, raw tail.
    struct ElementForAnalysis {spQuad quad;float distance=0,scale=1;std::uint32_t color=0xffffffff;};
    [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
    [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
    bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
    [[nodiscard]] const ElementForAnalysis& GetPrimaryForAnalysis() const noexcept{return primary_;}
    [[nodiscard]] const std::vector<std::unique_ptr<ElementForAnalysis>>& GetElementsForAnalysis() const noexcept{return elements_;}
    [[nodiscard]] float GetOcclusionRadiusForAnalysis() const noexcept{return radius_;}
    [[nodiscard]] float GetOcclusionSpeedForAnalysis() const noexcept{return speed_;}
    [[nodiscard]] spRenderNode* GetRenderNodeForAnalysis() const noexcept{return renderNode_;}
    [[nodiscard]] const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept override;
private:
    friend class spLensFlareSerializer;
    // PS2 destructor1B4270 destroys the array, primary Quad, then Renderable.
    ElementForAnalysis primary_;
    std::vector<std::unique_ptr<ElementForAnalysis>> elements_;
    float radius_=1,speed_=1;
    spRenderNode* renderNode_=nullptr; // PC4D9208 borrowed, null accepted
    mutable BoundingSphere sphere_{};
};
}
