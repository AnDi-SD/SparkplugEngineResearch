#pragma once
#include "wxEntityHost.h"

namespace winx::reconstruction
{
    class wxHUDPart;
    // Foreign SubscriptionManager, HUDManager and clone-manager dependencies.
    // The Part's borrowed vector, matching, lookup and selection stay in source.
    class wxHUDPartHost : public wxEntityHost
    {
    public:
        // Resolve may enter foreign startup. Null means that context remains
        // unqualified; the source reports the boundary without a fake removal.
        virtual void* ResolveHUDManagerForPartForAnalysis() noexcept = 0;
        virtual void RemoveHUDPartFromManagerForAnalysis(void* manager,std::uint32_t partKey) noexcept = 0;
        virtual void CompleteHUDPartCopyForAnalysis(std::uint32_t classID) const = 0;
        virtual void UnrecoveredHUDPartDependencyForAnalysis(
            wxHUDPart&,const void* notification,const char* reason) noexcept = 0;
    };
}
