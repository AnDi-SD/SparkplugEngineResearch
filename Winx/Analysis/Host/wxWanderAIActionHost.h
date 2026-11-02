#pragma once
#include <optional>

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    // Storage-only binding to owner24 -> entity24 -> exact plain Node. All
    // records stay live and stable through this synchronous operation. The
    // host does not synthesize constructor, PathFinder or native fault behavior.
    class wxWanderAIActionHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedBorrowedPosition };
        virtual ~wxWanderAIActionHost() = default;
        virtual ProfileForAnalysis EntryProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        // nullopt is an unavailable owner/entity wrapper; an engaged null is
        // a captured null Node link. Neither is admitted native fault behavior.
        virtual std::optional<sparkplug::reconstruction::spNode*>
            ReadOwnerNodeForAnalysis(void* capturedOwner) noexcept = 0;
    };
}
