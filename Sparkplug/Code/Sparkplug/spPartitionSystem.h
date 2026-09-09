#pragma once
// Inferred path. PC48E7C0 physically constructs RenderNode, but its original
// RTTI parent is Node. Keep these two independent facts; no RTTI substitution.
#include "spRenderNode.h"
#include "spPartitionNode.h"
namespace sparkplug::reconstruction
{
    class spPartitionSystem : public spRenderNode
    {
    public:
        static constexpr spClassID ClassID=0x912CC341;
        spPartitionSystem() noexcept;
        ~spPartitionSystem() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] spPartitionNode* GetPartitionRootForAnalysis() const noexcept{return root_.get();}
        // Native reader44AFCB stores directly, without deriving placement or
        // Scene state. Host refuses replacing an existing distinct direct owner.
        [[nodiscard]] bool SetPartitionRootForAnalysis(std::unique_ptr<spPartitionNode> root) noexcept;
    private:
        std::unique_ptr<spPartitionNode> root_;
    };
}
