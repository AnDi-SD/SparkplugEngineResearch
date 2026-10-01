#pragma once

#include "wxEntityHost.h"

#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    class wxProjectileManager;
    struct wxProjectileManagerRecordForAnalysis;

    // Timer, scene setup, node calls and record updates are game services.
    class wxProjectileManagerHost : public wxEntityHost
    {
    public:
        virtual bool IsGamePausedForAnalysis() const noexcept = 0;
        virtual std::uint32_t GameTimeForAnalysis() const noexcept = 0;
        // PC 505E2F / PS2 2C6654 calls an external manager service before
        // clearing the scene pointer and releasing records. Its source name
        // and full manager effect are still unknown.
        virtual void BeforePoolTeardownForAnalysis(wxProjectileManager&) noexcept = 0;
        virtual void SetupProjectileManagerForAnalysis(wxProjectileManager&) noexcept = 0;
        // Native records are 12 bytes. The host owns their actual storage.
        virtual wxProjectileManagerRecordForAnalysis* AllocatePoolRecordForAnalysis(
            wxProjectileManager&) noexcept = 0;
        // PC 506161 / PS2 2C51A0: node Enable(0,1), zero translation,
        // set dirty bit, then write scale to all three components.
        virtual void InitializePoolPayloadForAnalysis(void* payload,
            float scale) noexcept = 0;
        virtual void FreePoolRecordForAnalysis(
            wxProjectileManagerRecordForAnalysis&) noexcept = 0;
        virtual void UpdatePoolRecordForAnalysis(wxProjectileManager&,
            std::size_t group, std::size_t slot) noexcept = 0;
        virtual void DisablePoolPayloadForAnalysis(void* payload,
            std::uint32_t enabled, std::uint32_t immediate) noexcept = 0;
        // PC 5066A5 / PS2 2C6474, after Enable returns: reload the payload,
        // zero node translation and set its dirty bit. Scale is preserved.
        virtual void ResetPoolPayloadPositionForAnalysis(void* payload) noexcept = 0;
    };
}
