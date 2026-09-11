#pragma once
// PC4423D0/4423B0/4423C0 directly delegate to RenderNode read/write/index.
#include "spRenderNodeSerializer.h"
#include "spTextNode.h"
namespace sparkplug::reconstruction {
class spTextNodeSerializer final : public spRenderNodeSerializer {
public:
    static constexpr spClassID ClassID=0x46253465; // catalog/PC75EEF0, PS24AA9F0
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    spClassID GetTargetClassIDForAnalysis() const noexcept override{return spTextNode::ClassID;}
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
    bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
};
}
