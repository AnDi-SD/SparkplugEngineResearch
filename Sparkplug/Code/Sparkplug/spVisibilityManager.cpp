#include "spVisibilityManager.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateVisibilityManager()
        {
            return std::make_unique<spVisibilityManager>();
        }
        const spRTTIRecord VisibilityRecord{
            spVisibilityManager::ClassID, spBaseObject::ClassID,    "spVisibilityManager",
            &spBaseObject::StaticRTTI(),  &CreateVisibilityManager, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(VisibilityRecord);
    } // namespace
    const spRTTIRecord& spVisibilityManager::StaticRTTI() noexcept
    {
        (void)Registered;
        return VisibilityRecord;
    }
    const spRTTIRecord& spVisibilityManager::vfunc_18() const noexcept
    {
        return VisibilityRecord;
    }
    std::unique_ptr<spBaseObject> spVisibilityManager::vfunc_10(spCloneManager& manager) const
    {
        // Static original46D220: fresh factory + inherited Base copy. Whole
        // native factory remains capped; this is not a native ctor test claim.
        auto clone = std::make_unique<spVisibilityManager>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    void spVisibilityManager::BeginFrameForAnalysis(std::uint32_t& sceneStamp) noexcept
    {
        ++frameStamp_; // unsigned wrap, including the observed zero-stamp frame
        sceneStamp = frameStamp_;
        visible_.clear();
    }
    bool spVisibilityManager::TrySubmitForAnalysis(SupportForAnalysis& support,
                                                   const PlaneSetForAnalysis& planes,
                                                   const bool unclippedDebug)
    {
        const bool dynamic = support.kind == SupportKindForAnalysis::RenderNode;
        if ((!unclippedDebug && dynamic && !support.enabled) ||
            support.visibilityMark == frameStamp_)
            return false;
        if (!unclippedDebug && sphereCulling_ && !(dynamic && support.cullBypass) &&
            evidence::pc::visibility_math::IsOutside(planes, support.worldSphere))
            return false;
        visible_.push_back(&support);
        support.visibilityMark = frameStamp_;
        return true;
    }
} // namespace sparkplug::reconstruction
