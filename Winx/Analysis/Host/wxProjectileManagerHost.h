#pragma once

#include "wxEntityHost.h"

#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    class wxProjectileManager;

    // Timer, scene setup, node calls and record updates are game services.
    class wxProjectileManagerHost : public wxEntityHost
    {
    public:
        virtual bool IsGamePausedForAnalysis() const noexcept = 0;
        virtual std::uint32_t GameTimeForAnalysis() const noexcept = 0;
        virtual void SetupProjectileManagerForAnalysis(wxProjectileManager&) noexcept = 0;
        virtual void BeginPoolRegistrationForAnalysis(wxProjectileManager&,
            std::size_t group, std::size_t slot, void* payload) noexcept = 0;
        virtual void UpdatePoolRecordForAnalysis(wxProjectileManager&,
            std::size_t group, std::size_t slot) noexcept = 0;
        virtual void DisablePoolPayloadForAnalysis(void* payload,
            std::uint32_t enabled, std::uint32_t immediate) noexcept = 0;
    };
}
