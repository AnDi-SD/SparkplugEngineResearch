#pragma once

// Inferred platform declaration path; the original path is not present in
// the shipped executable.

#include "../Sparkplug/spCamera.h"

namespace sparkplug::reconstruction
{
    class spPS2Camera final : public spCamera
    {
    public:
        static constexpr spClassID ClassID = 0x055A04E0;

        spPS2Camera() noexcept = default;
        ~spPS2Camera() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
