#pragma once

#include "Analysis/Host/wxBacoProjectileManagerHost.h"
#include "Code/SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    struct wxBacoProjectileManagerMessageForAnalysis final
    {
        std::uint32_t code;
    };

    // Native parent is wxProjectileManager (PC 0x1D0, PS2 0x1E0).
    // The common parent is partially recovered. This leaf still uses its
    // existing scene adapter and spNamedObject C++ base until the two host
    // contracts can be bound without guessing scene or pool ownership.
    class wxBacoProjectileManager final : public sparkplug::reconstruction::spNamedObject
    {
    public:
        using Position = wxBacoProjectileManagerHost::Position;
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6F925DB7;

        wxBacoProjectileManager() noexcept = default;
        ~wxBacoProjectileManager() override;
        wxBacoProjectileManager(const wxBacoProjectileManager&) = delete;
        wxBacoProjectileManager& operator=(const wxBacoProjectileManager&) = delete;

        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        void SetHostForAnalysis(wxBacoProjectileManagerHost* host) noexcept
        { if (!setup_) host_ = host; }
        bool SetupForAnalysis() noexcept;
        bool UpdateForAnalysis() noexcept;
        bool FireTowardPlayerForAnalysis() noexcept;
        bool FireDirectionForAnalysis(Position direction) noexcept;
        void* GetProjectileForAnalysis(std::size_t index) const noexcept
        { return index < projectiles_.size() ? projectiles_[index] : nullptr; }
        void* GetEmitterForAnalysis(std::size_t index) const noexcept
        { return index < emitters_.size() ? emitters_[index] : nullptr; }

    private:
        static void NormalizeForAnalysis(Position& direction) noexcept;
        wxBacoProjectileManagerHost* host_ = nullptr; // borrowed
        std::array<void*, 2> projectiles_{}; // owned through host
        std::array<void*, 2> emitters_{}; // borrowed
        bool setup_ = false;
    };
}
