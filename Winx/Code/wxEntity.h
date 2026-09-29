#pragma once

#include "Code/Sparkplug/spEntity.h"
#include "Analysis/Host/wxEntityHost.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    // The own-byte array is an analytical PC-offset view; it is never
    // overlaid on the portable C++ object.
    class wxEntity : public sparkplug::reconstruction::spEntity
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x796A1869;
        using OwnBytesForAnalysis = std::array<std::uint8_t, 0x124 - 0x28>;

        explicit wxEntity(wxEntityHost& host);
        ~wxEntity() override;
        wxEntity(const wxEntity&) = delete;
        wxEntity& operator=(const wxEntity&) = delete;

        static void SetFactoryHostForAnalysis(wxEntityHost* host) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        [[nodiscard]] OwnBytesForAnalysis& GetOwnBytesForAnalysis() noexcept
        { return ownBytes_; }
        [[nodiscard]] const OwnBytesForAnalysis& GetOwnBytesForAnalysis() const noexcept
        { return ownBytes_; }
        [[nodiscard]] std::uint32_t ComputeFlagsForAnalysis() const;
        void UpdateFlagsForAnalysis();

    private:
        wxEntityHost& host_;
        OwnBytesForAnalysis ownBytes_{};
    };
}
