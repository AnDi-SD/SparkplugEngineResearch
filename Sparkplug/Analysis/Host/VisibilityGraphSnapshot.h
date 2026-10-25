#pragma once

// Our synchronous lifetime bridge, not an original engine class or native ABI.
#include "Code/Sparkplug/spVisibilityManager.h"

namespace sparkplug::host
{
    class VisibilityGraphSnapshot final
    {
    public:
        using Manager=reconstruction::spVisibilityManager;
        enum class Status{Complete,InvalidInput,HostLimit,AlreadyAttempted};
        VisibilityGraphSnapshot()=default;
        VisibilityGraphSnapshot(const VisibilityGraphSnapshot&)=delete;
        VisibilityGraphSnapshot& operator=(const VisibilityGraphSnapshot&)=delete;
        // One frame per snapshot. Source nodes, models, memberships, zones,
        // portals and captured camera caches must remain alive and unchanged
        // during processing. World/model producers run before Build; this
        // bridge reads their actual current caches and never fakes an update.
        // Build follows actual children, Zone roots and portal destinations;
        // caller does not supply substitute support or occluder lists.
        [[nodiscard]] Status Build(reconstruction::spPartitionNode& root);
        [[nodiscard]] Manager::TraversalSceneForAnalysis& Scene() noexcept{return scene_;}
        // Publish both completed and partial frame marks after concrete CPU
        // traversal/occlusion. Occluder/portal marks already mutate actual
        // source objects directly. False means there was no completed Build.
        [[nodiscard]] bool PublishMarks() noexcept;
        [[nodiscard]] reconstruction::spBaseObject* SourceOf(
            const Manager::SupportForAnalysis*) const noexcept;
        [[nodiscard]] bool Ready() const noexcept{return ready_;}
        // Manager outputs borrow this snapshot's support records: the snapshot
        // MUST outlive every use of those outputs. It retains no source owner.
    private:
        struct Binding{reconstruction::spBaseObject* source;Manager::SupportKindForAnalysis kind;};
        [[nodiscard]] Manager::SupportForAnalysis* Bind(reconstruction::spBaseObject*,
            Manager::SupportKindForAnalysis);
        bool attempted_=false,ready_=false;
        Manager::TraversalSceneForAnalysis scene_;
        std::vector<Manager::TraversalNodeForAnalysis> nodes_;
        std::vector<Manager::SupportForAnalysis> supports_;
        std::vector<Binding> bindings_;
    };
}
