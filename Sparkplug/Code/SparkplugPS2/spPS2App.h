#pragma once

// Inferred source/header path.  The shipped PS2 ELF preserves the exact class
// name but no original translation-unit path for spPS2App.

#include "../SparkBase/spApp.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spPS2App : public spApp
    {
    public:
        static constexpr spClassID ClassID = 0x354B1350;

        spPS2App() noexcept = default;
        ~spPS2App() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool vfunc_20_Initialize() override;
        [[nodiscard]] bool vfunc_24_Update() override = 0;
        void vfunc_28_Shutdown() override;
        [[nodiscard]] bool vfunc_2C_Run() override;
    };
}
