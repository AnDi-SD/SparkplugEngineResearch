#include "spZonePortalNode.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePortalNode()
        {
            return std::make_unique<spZonePortalNode>();
        }
        const spRTTIRecord Record{spZonePortalNode::ClassID, spNode::ClassID,   "spZonePortalNode",
                                  &spNode::StaticRTTI(),     &CreatePortalNode, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    } // namespace
    const spRTTIRecord& spZonePortalNode::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spZonePortalNode::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spZonePortalNode::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spZonePortalNode>();
        manager.RegisterClone(*this, *clone);
        return spNode::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spZonePortalNode::AppendPortalForAnalysis(spZonePortal* portal)
    {
        if (portals_.size() >= 4096)
            return false;
        portals_.push_back(portal);
        return true;
    }
    spZonePortal* spZonePortalNode::GetZonePortal(std::size_t index) const noexcept
    {
        return index < portals_.size() ? portals_[index] : nullptr;
    }
    const std::vector<spZonePortal*>& spZonePortalNode::GetPortalsForAnalysis() const noexcept
    {
        return portals_;
    }
} // namespace sparkplug::reconstruction
