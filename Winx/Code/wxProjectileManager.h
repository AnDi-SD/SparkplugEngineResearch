#pragma once

#include "wxEntity.h"
#include "Analysis/Host/wxProjectileManagerHost.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace winx::reconstruction
{
    struct wxProjectileManagerMessageForAnalysis final
    {
        std::uint32_t code = 0;
        std::uint32_t group = 0; // native message +18
        void* payload = nullptr; // native message +1C
    };

    // An adapter view of the native 12-byte record, never overlaid on it.
    struct wxProjectileManagerRecordForAnalysis final
    {
        std::uint32_t deadline = 0;
        std::uint8_t active = 0;
        void* payload = nullptr;
    };

    class wxProjectileManager : public wxEntity
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x7DB63B02;
        using OwnBytesForAnalysis = std::array<std::uint8_t, 0x170 - 0x124>;
        using PoolForAnalysis = std::array<std::array<
            wxProjectileManagerRecordForAnalysis*, 5>, 4>;

        explicit wxProjectileManager(wxProjectileManagerHost& host);
        ~wxProjectileManager() override;
        wxProjectileManager(const wxProjectileManager&) = delete;
        wxProjectileManager& operator=(const wxProjectileManager&) = delete;

        static void SetFactoryHostForAnalysis(wxProjectileManagerHost* host) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;
        void vfunc_0C(const void* notification) noexcept override;

        [[nodiscard]] OwnBytesForAnalysis& GetOwnBytesForAnalysis() noexcept { return ownBytes_; }
        [[nodiscard]] const OwnBytesForAnalysis& GetOwnBytesForAnalysis() const noexcept { return ownBytes_; }
        [[nodiscard]] PoolForAnalysis& GetPoolForAnalysis() noexcept { return pool_; }
        [[nodiscard]] const PoolForAnalysis& GetPoolForAnalysis() const noexcept { return pool_; }
        void SetEnabledForAnalysis(std::uint8_t enabled) noexcept { enabled_ = enabled; }
        [[nodiscard]] std::uint8_t GetEnabledForAnalysis() const noexcept { return enabled_; }
        [[nodiscard]] std::optional<std::size_t> FindFreeSlotForAnalysis(
            std::size_t group) const noexcept;
        [[nodiscard]] bool TickForAnalysis() noexcept;

    private:
        wxProjectileManagerHost& host_;
        OwnBytesForAnalysis ownBytes_{};
        PoolForAnalysis pool_{}; // record storage allocated/freed by host
        std::uint8_t enabled_ = 0;
    };
}
