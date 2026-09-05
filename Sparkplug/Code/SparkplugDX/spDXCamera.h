#pragma once

// Inferred platform declaration path; the original path is not present in
// either shipped executable.

#include "../Sparkplug/spCamera.h"

namespace sparkplug::reconstruction
{
    class spDXCamera final : public spCamera
    {
    public:
        static constexpr spClassID ClassID = 0x41672E34;

        spDXCamera() noexcept = default;
        ~spDXCamera() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
