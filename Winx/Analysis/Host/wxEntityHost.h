#pragma once

#include "Code/Sparkplug/spEntity.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction { class spCloneManager; }

namespace winx::reconstruction
{
    class wxEntity;

    // spEntity, two manager lists, and the bound game record are external.
    class wxEntityHost : public sparkplug::reconstruction::spEntityHost
    {
    public:
        virtual ~wxEntityHost() = default;
        virtual void MoveFromEngineToGameForAnalysis(wxEntity& entity) = 0;
        virtual void DestroyWxEntityForAnalysis(wxEntity& entity) noexcept = 0;
        virtual void CompleteCopyForAnalysis(const wxEntity& source,
            wxEntity& destination,
            sparkplug::reconstruction::spCloneManager& manager,
            std::uint32_t classID) const = 0;
        virtual std::size_t CachedPredicateCountForAnalysis(const wxEntity& entity) const = 0;
        virtual bool CachedPredicateForAnalysis(const wxEntity& entity,
            std::size_t index) const = 0;
        virtual bool TimerByteForAnalysis(const wxEntity& entity) const = 0;
        virtual float SquaredDistanceForAnalysis(const wxEntity& entity) const = 0;
        virtual void WriteComputedFlagsForAnalysis(wxEntity& entity,
            std::uint32_t flags) = 0;
    };
}
