#pragma once

// Inferred game-source location; the ELF preserves the class name but no
// original wxPS2App translation-unit path.

#include "Code/SparkplugPS2/spPS2App.h"

#include <memory>

namespace winx::reconstruction
{
    class wxPS2App final : public sparkplug::reconstruction::spPS2App
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x36973698;

        wxPS2App() = default;
        ~wxPS2App() override;

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
    };
}
