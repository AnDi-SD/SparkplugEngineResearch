#pragma once

#include <array>
#include <cstddef>

namespace winx::reconstruction
{
    class wxBacoProjectileManager;

    // Scene lookup, projectile construction and projectile virtual calls are
    // outside the recovered class. The two created projectiles belong to the
    // manager; the host must outlive it after SetupForAnalysis succeeds.
    class wxBacoProjectileManagerHost
    {
    public:
        using Position = std::array<float, 3>;
        virtual ~wxBacoProjectileManagerHost() = default;
        virtual bool HasSceneForAnalysis(const wxBacoProjectileManager&) noexcept = 0;
        virtual void AttachSceneForAnalysis(wxBacoProjectileManager&) noexcept = 0;
        virtual void* FindEmitterForAnalysis(std::size_t index) noexcept = 0;
        virtual void* CreateProjectileForAnalysis(std::size_t index) noexcept = 0;
        virtual void BaseSetupForAnalysis(wxBacoProjectileManager&) noexcept = 0;
        virtual void DestroyProjectileForAnalysis(void*) noexcept = 0;
        virtual bool IsProjectileActiveForAnalysis(void*) noexcept = 0;
        virtual void LaunchProjectileForAnalysis(void*, const Position& origin,
            const Position& direction) noexcept = 0;
        virtual void UpdateProjectileForAnalysis(void*) noexcept = 0;
        virtual Position GetEmitterPositionForAnalysis(void*) noexcept = 0;
        virtual bool TryGetPlayerPositionForAnalysis(Position&) noexcept = 0;
    };
}
