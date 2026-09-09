#pragma once
// Inferred path. Actual PC480FD0 Node-derived factory; borrowed root vector
//481080 and Node-only clone481030. No scene registration/traversal is implied.
#include "spNode.h"
namespace sparkplug::reconstruction
{
    class spPartitionNode;
    class spZone : public spNode
    {
    public:
        static constexpr spClassID ClassID=0x61254AB3;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Duplicate borrowed roots are preserved; no retain/release/delete.
        [[nodiscard]] bool AppendRootForAnalysis(spPartitionNode* root);
        [[nodiscard]] const std::vector<spPartitionNode*>& GetRootsForAnalysis() const noexcept{return roots_;}
    private:
        std::vector<spPartitionNode*> roots_;
    };
}
