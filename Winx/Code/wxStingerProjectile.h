#pragma once

#include "wxProjectile.h"

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxStingerProjectile final : public wxProjectile
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x206A45A2;
        using TailForAnalysis = std::array<std::uint8_t, 12>;

        explicit wxStingerProjectile(wxProjectileHost& host);
        ~wxStingerProjectile() override;
        wxStingerProjectile(const wxStingerProjectile&) = delete;
        wxStingerProjectile& operator=(const wxStingerProjectile&) = delete;

        static void SetFactoryHostForAnalysis(wxProjectileHost* host) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        [[nodiscard]] TailForAnalysis& GetTailForAnalysis() noexcept { return tail_; }
        [[nodiscard]] const TailForAnalysis& GetTailForAnalysis() const noexcept { return tail_; }

    private:
        TailForAnalysis tail_{}; // native PC/PS2 +EC..+F7
    };
}
