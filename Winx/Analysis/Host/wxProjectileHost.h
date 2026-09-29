#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    class wxProjectile;

    // Native reference objects and spActor require the game runtime.
    class wxProjectileHost
    {
    public:
        virtual ~wxProjectileHost() = default;
        virtual std::uint16_t ReferenceCountForAnalysis(std::uint32_t token) = 0;
        virtual void SetReferenceCountForAnalysis(std::uint32_t token,
            std::uint16_t count) = 0;
        virtual void DeleteReferenceForAnalysis(std::uint32_t token) = 0;
        virtual void* CreateFreshActorForAnalysis() = 0;
        virtual void DestroyActorForAnalysis(void* actor) = 0;
        virtual void DestroyProjectileForAnalysis(wxProjectile&) noexcept = 0;
        virtual void CompleteCopyForAnalysis(const wxProjectile&,
            wxProjectile&, std::uint32_t classID) = 0;
    };
}
