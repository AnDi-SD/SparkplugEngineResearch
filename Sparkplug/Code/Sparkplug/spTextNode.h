#pragma once
// Inferred path. PC41A5E0/437890 and original slots437970/437A10/4379D0.
#include "spRenderNode.h"
namespace sparkplug::reconstruction {
class spTextRenderable;
class spTextNode final : public spRenderNode {
public:
    static constexpr spClassID ClassID=0x52E86EFE;
    ~spTextNode() override;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override {return nullptr;}
    bool vfunc_14(spBaseObject&,spCloneManager&) const override {return false;}
    bool AttachRenderableForAnalysis(std::shared_ptr<spRenderable>) override;
    std::shared_ptr<spRenderable> DetachRenderableForAnalysis(spRenderable&) noexcept override;
    void ClearRenderablesForAnalysis() noexcept override;
    spTextRenderable* GetTextRenderableForAnalysis() const noexcept {return text_.get();}
private:
    // Native1D4 starts NULL; its producer remains outside this slice.
    std::shared_ptr<spBaseObject> auxiliary_;
    std::shared_ptr<spTextRenderable> text_; //1D8, last attached Text, separate owning ref
};
}
