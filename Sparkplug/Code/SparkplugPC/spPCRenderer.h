#pragma once

// Inferred platform header path; no exact spPCRenderer source path survives.

#include "../SparkplugDX/spDXRenderer.h"

namespace sparkplug::reconstruction
{
    class spPCRenderer final : public spDXRenderer
    {
    public:
        static constexpr spClassID ClassID = 0x26267C84;

        spPCRenderer() = default;
        ~spPCRenderer() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
