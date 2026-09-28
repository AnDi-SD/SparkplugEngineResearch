#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxBacoManager;

    // Calls outside the recovered manager: game clock, RNG and scene effects.
    class wxBacoManagerHost
    {
    public:
        virtual ~wxBacoManagerHost() = default;
        virtual std::uint32_t GetTickForAnalysis() noexcept = 0;
        virtual std::uint32_t NextRandomForAnalysis() noexcept = 0;
        virtual std::array<float, 3> GetPlayerPositionForAnalysis() noexcept = 0;
        virtual void SetupSceneForAnalysis(wxBacoManager&) noexcept = 0;
        // Native 573100 applies a scene transform, activates the member and
        // sends message 2725. The manager itself advances the counter/timer.
        virtual void SpawnMemberForAnalysis(wxBacoManager&, void* member,
            std::uint32_t slot) noexcept = 0;
    };
}
