#pragma once

#include "Analysis/Host/wxProjectileHost.h"
#include "Code/SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    // Native registration derives from wxEntity, while the measured PC Copy
    // calls spNamedObject directly. This portable view keeps that exact Copy
    // boundary and represents all native offsets as analytical bytes.
    class wxProjectile : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1ACF3C70;
        using BytesForAnalysis = std::array<std::uint8_t, 0xEC>;

        explicit wxProjectile(wxProjectileHost& host);
        ~wxProjectile() override;
        wxProjectile(const wxProjectile&) = delete;
        wxProjectile& operator=(const wxProjectile&) = delete;

        static void SetFactoryHostForAnalysis(wxProjectileHost* host) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        [[nodiscard]] BytesForAnalysis& GetBytesForAnalysis() noexcept { return bytes_; }
        [[nodiscard]] const BytesForAnalysis& GetBytesForAnalysis() const noexcept { return bytes_; }
        [[nodiscard]] void* GetActorForAnalysis() const noexcept { return actor_; }

    private:
        void TransferReferenceForAnalysis(wxProjectile& target,
            std::size_t offset) const;
        wxProjectileHost& host_;
        BytesForAnalysis bytes_{};
        void* actor_ = nullptr; // owned through host; native +20
    };
}
