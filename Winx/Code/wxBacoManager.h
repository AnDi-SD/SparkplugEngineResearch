#pragma once

#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxBacoManagerHost.h"

#include <array>
#include <cstdint>
#include <vector>

namespace winx::reconstruction
{
    // Portable message view. Native PC code reads code at +0 and payload at +10.
    struct wxBacoManagerMessageForAnalysis final
    {
        std::uint32_t code;
        void* payload = nullptr;
    };

    // Native physical parent is wxEntity; its shared game implementation is
    // still open. The portable parent preserves the confirmed RTTI ancestry.
    class wxBacoManager final : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x352C4347;
        using Position = std::array<float, 3>;
        struct StateForAnalysis final
        {
            std::array<std::uint32_t, 6> deadlines{};
            std::uint32_t nextProximityCheck = 0;
            bool reachedFirst = false, reachedSecond = false;
            bool waveFirst = false, waveSecond = false;
            std::uint32_t firstCount = 0, secondCount = 0;
        };

        wxBacoManager() noexcept = default;
        ~wxBacoManager() override = default;
        wxBacoManager(const wxBacoManager&) = delete;
        wxBacoManager& operator=(const wxBacoManager&) = delete;

        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        void SetHostForAnalysis(wxBacoManagerHost* host) noexcept { host_ = host; }
        void SetupForAnalysis() noexcept;
        bool UpdateForAnalysis() noexcept;
        void CheckProximityForAnalysis(const Position& player) noexcept;
        const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        const std::vector<void*>& GetMembersForAnalysis() const noexcept { return members_; }

    private:
        void ProcessWaveForAnalysis(std::uint32_t wave) noexcept;
        wxBacoManagerHost* host_ = nullptr; // borrowed platform adapter
        bool sceneSetup_ = false;
        StateForAnalysis state_;
        std::vector<void*> members_;
    };
}
