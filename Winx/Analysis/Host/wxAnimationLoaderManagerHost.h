#pragma once
#include "wxAnimationLoaderHost.h"
#include "Code/wxAnimationManager.h"

namespace winx::reconstruction
{
    // Our optional bridge: entity and context stay application dependencies;
    // animation-set operations use the one reconstructed manager implementation.
    class wxAnimationLoaderManagerHost : public wxAnimationLoaderHost
    {
    public:
        // Must resolve the current singleton, creating it when absent.
        virtual wxAnimationManager& ResolveAnimationManagerForAnalysis() noexcept = 0;
        void* GetAnimationManagerForAnalysis() noexcept final
        { return &ResolveAnimationManagerForAnalysis(); }
        void LoadSetForAnalysis(void* manager, std::uint32_t set) noexcept final
        { static_cast<wxAnimationManager*>(manager)->LoadSetForAnalysis(set); }
        void ReleaseSetForAnalysis(void* manager, std::uint32_t set) noexcept final
        { static_cast<wxAnimationManager*>(manager)->ReleaseSetForAnalysis(set); }
    };
}
