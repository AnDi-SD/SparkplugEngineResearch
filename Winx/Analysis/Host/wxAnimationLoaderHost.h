#pragma once
#include "Code/SparkBase/spBaseObject.h"

namespace winx::reconstruction
{
    class wxAnimationLoader;

    // Our adapter boundary for the absent wxEntity/game/animation-manager code.
    // Handles are borrowed. GetAnimationManager must resolve the current
    // singleton on every call, including its original lazy creation behavior.
    class wxAnimationLoaderHost
    {
    public:
        virtual ~wxAnimationLoaderHost() = default;
        virtual void ConstructEntityForAnalysis(wxAnimationLoader&, bool) = 0;
        virtual void DestroyEntityForAnalysis(wxAnimationLoader&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxAnimationLoader&, wxAnimationLoader&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual std::uint32_t GetCurrentLevelForAnalysis() noexcept = 0;
        virtual void* GetAnimationManagerForAnalysis() noexcept = 0;
        virtual void LoadSetForAnalysis(void* manager, std::uint32_t set) noexcept = 0;
        virtual void ReleaseSetForAnalysis(void* manager, std::uint32_t set) noexcept = 0;
    };
}
