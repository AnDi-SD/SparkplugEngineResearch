#pragma once

// Inferred game-source location.  The exact game PDB root is partially known,
// but no translation-unit path for wxPCApp survives in the shipped image.

#include "Code/SparkplugPC/spPCApp.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace winx::reconstruction
{
    class wxPCApp final : public sparkplug::reconstruction::spPCApp
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x707D09F3;

        wxPCApp() = default;
        ~wxPCApp() override;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;
        [[nodiscard]] const char* vfunc_GetEmptyString() const noexcept override;

        [[nodiscard]] bool vfunc_20_Initialize() override;
        [[nodiscard]] bool vfunc_24_Update() override;
        void vfunc_28_Shutdown() override;

        // Host-neutral seam for the native lazy sprintf at 0x0040E250.
        void SetBuildLabelsForAnalysis(
            std::string_view version,
            std::string_view buildLabel);
        [[nodiscard]] bool IsShutdownRequestedForAnalysis() const noexcept;

    private:
        mutable std::string windowTitle_ = "Winx PC";
        bool shutdownRequested_ = false;
    };
}
