#include "spPartitionNode.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePartitionNode()
        {
            return std::make_unique<spPartitionNode>();
        }
        const spRTTIRecord Record{spPartitionNode::ClassID, spBaseObject::ClassID,
                                  "spPartitionNode",        &spBaseObject::StaticRTTI(),
                                  &CreatePartitionNode,     nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    } // namespace
    spPartitionNode::spPartitionNode(std::size_t childCount) : children_(childCount)
    {
    }
    const spRTTIRecord& spPartitionNode::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spPartitionNode::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spPartitionNode::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPartitionNode>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    spPartitionNode* spPartitionNode::FindLeafForAnalysis(const Vector3&, bool) noexcept
    {
        return this; // PC425680 ignores both arguments
    }
    std::size_t spPartitionNode::GetChildCountForAnalysis() const noexcept
    {
        return children_.size();
    }
    spPartitionNode* spPartitionNode::GetChildForAnalysis(std::size_t index) noexcept
    {
        return index < children_.size() ? children_[index].get() : nullptr;
    }
    bool spPartitionNode::SetChildForAnalysis(std::size_t index,
                                              std::unique_ptr<spPartitionNode> child) noexcept
    {
        if (index >= children_.size())
            return false;
        children_[index] = std::move(child);
        return true;
    }
    void spPartitionNode::SetZonePresentForAnalysis(bool present) noexcept
    {
        zonePresent_ = present;
    }
    bool spPartitionNode::HasZoneForAnalysis() const noexcept
    {
        return zonePresent_;
    }
} // namespace sparkplug::reconstruction
